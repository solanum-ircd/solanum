/*
 * Solanum: a slightly advanced ircd
 * cap_labeled_response.c: labeled-response hooks
 *
 * Copyright (c) 2026 Ryan Schmidt <skizzerz@skizzerz.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "stdinc.h"
#include "hash.h"
#include "modules.h"
#include "response.h"
#include "send.h"
#include "s_serv.h"

#define IsMeOrServer(x) (IsMe(x) || IsServer(x))

static const char cap_labeled_response_desc[] =
	"Provides the labeled-response client capability";

static bool cap_receive_label_visible(struct Client *);
static void cap_labeled_response_incoming(void *);
static void cap_labeled_response_process(void *);
static void cap_labeled_response_cleanup(void *);
static void me_ack(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

static const char *serv_response_tag = "solanum.chat/response";
static char response_tag_buf[BUFSIZE];
/* other modules may modify outgoing_response_info, so keep track of who we add CLICAP_RECEIVE_LABEL to here */
static struct Client *label_eligible = NULL;

static uint64_t CLICAP_LABELED_RESPONSE;
static uint64_t CLICAP_RECEIVE_LABEL;

static struct ClientCapability capdata_receive_label = {
	.visible = cap_receive_label_visible,
	.flags = CLICAP_FLAGS_NOPROP,
};

struct Message ack_msgtab = {
	"ACK", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_ack, 0}, mg_ignore}
};

mapi_clist_av1 cap_labeled_response_clist[] = { &ack_msgtab, NULL };

mapi_cap_list_av2 cap_labeled_response_cap_list[] = {
	{ MAPI_CAP_CLIENT, "?receive_label", &capdata_receive_label, &CLICAP_RECEIVE_LABEL },
	{ MAPI_CAP_CLIENT, "labeled-response", NULL, &CLICAP_LABELED_RESPONSE },
	{ 0, NULL, NULL, NULL },
};

mapi_hfn_list_av1 cap_labeled_response_hfnlist[] = {
	{ "message_tag", cap_labeled_response_incoming },
	{ "outbound_msgbuf", cap_labeled_response_process },
	{ "parse_end", cap_labeled_response_cleanup, HOOK_HIGHEST },
	{ NULL, NULL }
};

static bool
cap_receive_label_visible(struct Client *client)
{
	return false;
}

static void
cap_labeled_response_incoming(void *data_)
{
	hook_data_message_tag *data = data_;

	if (MyConnect(data->source)
		&& IsClientCapable(data->source, CLICAP_LABELED_RESPONSE | CLICAP_BATCH)
		&& !strcmp("label", data->key)
		&& !EmptyString(data->value)
		&& strlen(data->value) <= 64)
	{
		label_eligible = data->source;
		SetClientCap(data->source, CLICAP_RECEIVE_LABEL);
		outgoing_response_info = rb_malloc(sizeof(struct ResponseInfo));
		outgoing_response_info->source_p = data->source;
		outgoing_response_info->client_p = data->client;
		rb_strlcpy(outgoing_response_info->label, data->value, sizeof(outgoing_response_info->label));
		/* don't propagate label further, but approve the tag so modules like m_batch can make use of it */
		data->approved = MESSAGE_TAG_ALLOW;
		data->capmask = NOCAPS;
	}
	else if (IsServer(data->client) && !strcmp(serv_response_tag, data->key) && !EmptyString(data->value))
	{
		/* tag value should be UID,batch_id */
		char uid[IDLEN] = { 0 };
		const char *batch_id = strchr(data->value, ',');
		if (batch_id == NULL)
			return;

		++batch_id;
		memcpy(uid, data->value, sizeof(uid) - 1);
		struct Client *client = find_id(uid);
		if (client == NULL)
			return;

		if (MyConnect(client))
		{
			outgoing_response_info = get_remote_response_batch(batch_id);
			if (outgoing_response_info != NULL && IsClientCapable(outgoing_response_info->source_p, CLICAP_LABELED_RESPONSE | CLICAP_BATCH))
			{
				label_eligible = outgoing_response_info->source_p;
				SetClientCap(outgoing_response_info->source_p, CLICAP_RECEIVE_LABEL);
			}
		}
		else
		{
			outgoing_response_info = rb_malloc(sizeof(struct ResponseInfo));
			outgoing_response_info->source_p = client;
			outgoing_response_info->client_p = client->from;
			rb_strlcpy(outgoing_response_info->batch, batch_id, sizeof(outgoing_response_info->batch));
			outgoing_response_info->sent = true;
		}

		data->approved = MESSAGE_TAG_ALLOW;
		data->capmask = CLICAP_SERVONLY;
	}
}

static void
cap_labeled_response_process(void *data_)
{
	hook_data_outbound_msgbuf *data = data_;
	struct MsgBuf *msgbuf = data->msgbuf;
	const char *response_tag = NULL;

	if (incoming_message != NULL)
		response_tag = msgbuf_get_tag(incoming_message, serv_response_tag);

	if (outgoing_response_info != NULL && MyConnect(outgoing_response_info->source_p))
	{
		/* this assumes messages with a server as a source sent to a channel go to all members */
		bool attach_tags = data->source_sees_message
			|| outgoing_response_info->source_p == data->target
			|| (data->chptr != NULL && IsMeOrServer(data->source) && find_channel_membership(data->chptr, outgoing_response_info->source_p) != NULL);

		if (!outgoing_response_info->sent && attach_tags && !outgoing_response_info->skip_tags)
		{
			outgoing_response_info->sent = true;
			msgbuf_append_tag(msgbuf, "label", outgoing_response_info->label, CLICAP_RECEIVE_LABEL);
		}

		if (attach_tags
			&& !outgoing_response_info->skip_tags
			&& !EmptyString(outgoing_response_info->batch)
			&& msgbuf_get_tag(msgbuf, "batch") == NULL
			&& strcmp(msgbuf->cmd, "BATCH") != 0)
		{
			msgbuf_append_tag(msgbuf, "batch", outgoing_response_info->batch, CLICAP_RECEIVE_LABEL);
		}
	}

	/* propagate solanum.chat/response to other servers */
	if (response_tag != NULL)
	{
		msgbuf_append_tag(msgbuf, serv_response_tag, response_tag, CLICAP_SERVONLY);
	}
	else if (outgoing_response_info != NULL && outgoing_response_info->remote_response > 0)
	{
		snprintf(response_tag_buf, sizeof(response_tag_buf), "%s,%s",
			outgoing_response_info->source_p->id, outgoing_response_info->batch);
		msgbuf_append_tag(msgbuf, serv_response_tag, response_tag_buf, CLICAP_SERVONLY);
	}
}

static void
cap_labeled_response_cleanup(void *unused)
{
	if (outgoing_response_info != NULL)
	{
		if (MyConnect(outgoing_response_info->source_p))
		{
			/* send an ACK if the handlers didn't send anything */
			if (!outgoing_response_info->sent)
				sendto_one(outgoing_response_info->source_p, ":%s ACK", me.name);

			if (!EmptyString(outgoing_response_info->batch) && outgoing_response_info->remote_response == 0)
			{
				sendto_one(outgoing_response_info->source_p, ":%s BATCH -%s",
					me.name, outgoing_response_info->batch);
			}
		}
		else
		{
			/* notify remote that we're done sending responses to this command */
			sendto_one(outgoing_response_info->source_p, ":%s ENCAP %s ACK",
				me.id, outgoing_response_info->source_p->servptr->name);
		}

		if (outgoing_response_info->remote_response == 0)
			free_response_batch(outgoing_response_info);

		outgoing_response_info = NULL;
	}

	if (label_eligible != NULL)
	{
		ClearClientCap(label_eligible, CLICAP_RECEIVE_LABEL);
		label_eligible = NULL;
	}
}

static void
me_ack(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	/* client already left or something, so nothing to do here */
	if (outgoing_response_info == NULL)
		return;

	outgoing_response_info->remote_response--;
}

DECLARE_MODULE_AV2(cap_labeled_response, NULL, NULL, cap_labeled_response_clist, NULL, cap_labeled_response_hfnlist, cap_labeled_response_cap_list, NULL, cap_labeled_response_desc);
