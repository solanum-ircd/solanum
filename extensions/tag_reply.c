/*
 * Solanum: a slightly advanced ircd
 * tag_reply.c: implement the IRCv3 +reply client tag
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
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "client_tags.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"
#include "chmode.h"
#include "hash.h"
#include "newconf.h"
#include "parse.h"
#include "inline/stringops.h"

/* Minimum string length of a valid version 1 message id:
 * 1. The character '1' (1 character)
 * 2. Current seconds since epoch (10 characters)
 * 3. Current milliseconds value for current time (3 characters)
 * 4. Counter value (6 characters)
 * 5. Client UID (IDLEN-1 characters)
 * Total = 20 + IDLEN - 1 = 19 + IDLEN
 */
#define MSGID_LEN_MIN (19 + IDLEN)

static const char tag_reply_desc[] = "Provides support for the +reply client tag.";
static void conf_set_reply_advertise_fallback(void *);
static void conf_set_reply_enable_fallback(void *);
static void tag_reply_allow(void *);
static void tag_reply_apply_conf(void *);
static void tag_reply_conf_info(void *);
static void tag_reply_fallback(void *);
static void tag_reply_reset_conf(void *);

/* Require network opt-in for all fallback behaviour */
int enable_fallback = 0;
int advertise_fallback = 0;

mapi_hfn_list_av1 tag_reply_hfnlist[] = {
	{ "conf_read_end", tag_reply_apply_conf },
	{ "conf_read_start", tag_reply_reset_conf },
	{ "doing_info_conf", tag_reply_conf_info },
	{ "message_tag", tag_reply_allow },
	{ "outbound_msgbuf", tag_reply_fallback },
	{ NULL, NULL }
};

static int
modinit(void)
{
	add_client_tag("reply");
	add_conf_item("general", "reply_enable_fallback", CF_YESNO, conf_set_reply_enable_fallback);
	add_conf_item("general", "reply_advertise_fallback", CF_YESNO, conf_set_reply_advertise_fallback);
	return 0;
}

static void
moddeinit(void)
{
	remove_client_tag("reply");
	remove_client_tag("draft/reply");
	remove_conf_item("general", "reply_enable_fallback");
	remove_conf_item("general", "reply_advertise_fallback");
}

static void
conf_set_reply_advertise_fallback(void *data)
{
	advertise_fallback = *(int *)data;
}

static void
conf_set_reply_enable_fallback(void *data)
{
	enable_fallback = *(int *)data;
}

static void
tag_reply_allow(void *data_)
{
	hook_data_message_tag *data = data_;
	const char *target = data->message->para[1];

	/* fallback: rewrite +draft/reply into +reply as long as both don't exist */
	if (enable_fallback && !strcmp("+draft/reply", data->key) && msgbuf_get_tag(data->message, "+reply") == NULL)
		data->key = "+reply";

	if (strcmp("+reply", data->key) != 0 || EmptyString(data->value))
		return;

	/* If coming from a client, validate that the reply is a "valid" message id for the message target */
	if (MyClient(data->source))
	{
		if (NotClientCapable(data->source, CLICAP_MESSAGE_TAGS))
			return;

		/* not a message? */
		if (strcasecmp(data->message->cmd, "PRIVMSG") != 0
			&& strcasecmp(data->message->cmd, "NOTICE") != 0
			&& strcasecmp(data->message->cmd, "TAGMSG") != 0)
		{
			return;
		}

		/* unrecognized message id format? */
		size_t idlen = strlen(data->value);
		if (*data->value != '1' || idlen < MSGID_LEN_MIN)
			return;

		/* message lacking a target or sent to multiple targets? */
		if (data->message->n_para < 2 || EmptyString(target) || strchr(target, ',') != NULL)
		{
			return;
		}

		/* check if the target is a channel (possibly a statusmsg) */
		const char *ch_target = NULL;
		if (IsChannelName(target))
			ch_target = target;
		else if ((*target == '@' || *target == '+') && IsChannelName(target + 1))
			ch_target = target + 1;

		/* PMs have an idlen of exactly 29, channel messages are always > 29 */
		if ((ch_target == NULL) ^ (idlen == MSGID_LEN_MIN))
			return;

		/* quick validation of msgid portion before channel name */
		for (int i = 1; i < 29; i++)
		{
			if (isdigit(data->value[i]))
				continue;
			if (i >= 20 && isupper(data->value[i]))
				continue;
			return;
		}

		if (ch_target != NULL)
		{
			/* the target must match the channel name in the reply tag */
			int chlen;
			char *chname = rb_base64_decode(data->value + MSGID_LEN_MIN, idlen - MSGID_LEN_MIN, &chlen);
			if (chname == NULL)
				return;

			bool is_match = !irccmp(chname, ch_target);
			rb_free(chname);
			if (!is_match || find_channel(ch_target) == NULL)
				return;
		}
	}

	data->capmask = CLICAP_MESSAGE_TAGS;
	data->approved = MESSAGE_TAG_ALLOW;
}

static void
tag_reply_apply_conf(void *unused)
{
	if (enable_fallback && advertise_fallback)
		add_client_tag("draft/reply");
	else
		remove_client_tag("draft/reply");
}

static void
tag_reply_reset_conf(void *unused)
{
	enable_fallback = 0;
	advertise_fallback = 0;
}

static void
tag_reply_conf_info(void *data_)
{
	hook_data *data = data_;
	sendto_one(data->client, ":%s %d %s :%-30s %-16s [%s]",
		get_id(&me, data->client), RPL_INFO,
		get_id(data->client, data->client),
		"reply_enable_fallback",
		enable_fallback ? "YES" : "NO",
		"Enable +draft/reply fallback support for +reply");

	/* Everything after this point requires that fallback is enabled */
	if (!enable_fallback)
		return;

	sendto_one(data->client, ":%s %d %s :%-30s %-16s [%s]",
		get_id(&me, data->client), RPL_INFO,
		get_id(data->client, data->client),
		"reply_advertise_fallback",
		advertise_fallback ? "YES" : "NO",
		"Advertise +draft/reply in CLIENTTAGDENY");
}

static void
tag_reply_fallback(void *data_)
{
	hook_data_outbound_msgbuf *data = data_;
	const char *reply = msgbuf_get_tag(data->msgbuf, "+reply");

	if (enable_fallback && reply != NULL && msgbuf_get_tag(data->msgbuf, "+draft/reply") == NULL)
		msgbuf_append_tag(data->msgbuf, "+draft/reply", reply, CLICAP_MESSAGE_TAGS);
}

DECLARE_MODULE_AV2(tag_reply, modinit, moddeinit, NULL, NULL, tag_reply_hfnlist, NULL, NULL, tag_reply_desc);
