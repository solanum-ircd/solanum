/*
 * Solanum: a slightly advanced ircd
 * multiline.c: Implement the draft/multiline batch type
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
#include "batch.h"
#include "client.h"
#include "hash.h"
#include "modules.h"
#include "msgbuf.h"
#include "newconf.h"
#include "numeric.h"
#include "send.h"
#include "s_assert.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_serv.h"
#include "tgchange.h"

/* the MULTILN server capability indicates support for the s2s multiline batch type
 * since this is a proof-of-concept extension the s2s interface is UNSTABLE and may change incompatibly.
 * Networks need to ensure all servers have the same module version to avoid potential crashes.
 */
static uint64_t CAP_MULTILINE;
static uint64_t CLICAP_MULTILINE;
static int h_multiline_message_user;
static int h_multiline_message_channel;

static const char multiline_desc[] =
	"Allows clients to send batches containing multiple lines at once (draft/multiline batch type).";

static char capdata_buf[BUFSIZE];
static unsigned int old_max_lines = 0;
static unsigned int old_max_bytes = 0;
static unsigned int max_lines = 0;
static unsigned int max_bytes = 0;

static int multiline_modinit(void);
static void multiline_moddeinit(void);
static void process_multiline(struct Client *client_p, struct Client *source_p, struct Batch *batch, void *);
static void multiline_conf_info(void *);
static void multiline_conf_store(void *);
static void multiline_conf_update(void *);
static void multiline_tag_allow(void *);
static void multiline_rehash(void *);
static const char *multiline_config(struct Client *);

mapi_hfn_list_av1 multiline_hfn_list[] = {
	{ "conf_read_start", multiline_conf_store },
	{ "conf_read_end", multiline_conf_update },
	{ "doing_info_conf", multiline_conf_info },
	{ "message_tag", multiline_tag_allow },
	{ "rehash", multiline_rehash },
	{ NULL, NULL },
};

mapi_hlist_av1 multiline_hlist[] = {
	{ "multiline_message_user", &h_multiline_message_user },
	{ "multiline_message_channel", &h_multiline_message_channel },
	{ NULL, NULL },
};

struct ClientCapability capdata_multiline = { .data = multiline_config };

mapi_cap_list_av2 multiline_cap_list[] = {
	{ MAPI_CAP_CLIENT, "draft/multiline", &capdata_multiline, &CLICAP_MULTILINE },
	{ MAPI_CAP_SERVER, "MULTILN", NULL, &CAP_MULTILINE },
	{ 0, NULL, NULL, NULL },
};

struct BatchHandler multiline_handler = { process_multiline, NULL, 0, NULL };

static void
conf_set_multiline_max_bytes(void *data)
{
	max_bytes = *(int *)data;
}

static void
conf_set_multiline_max_lines(void *data)
{
	max_lines = *(int *)data;
}

static int
multiline_modinit(void)
{
	if (!register_batch_handler("draft/multiline", &multiline_handler))
		return -1;

	add_conf_item("general", "multiline_max_bytes", CF_INT, conf_set_multiline_max_bytes);
	add_conf_item("general", "multiline_max_lines", CF_INT, conf_set_multiline_max_lines);

	return 1;
}

static void
multiline_moddeinit(void)
{
	remove_batch_handler("draft/multiline");
}

static const char *
multiline_config(struct Client *unused)
{
	return capdata_buf;
}

static void
multiline_conf_store(void *unused)
{
	old_max_bytes = max_bytes;
	old_max_lines = max_lines;
}

static void
multiline_conf_update(void *unused)
{
	sprintf(capdata_buf, "max-bytes=%d,max-lines=%d", max_bytes, max_lines);
}

static void
multiline_rehash(void *data)
{
	/* config rehash may have changed multiline_max_bytes or multiline_max_lines */
	if (old_max_bytes != max_bytes || old_max_lines != max_lines)
	{
		/* notify cap-notify clients of the new limits */
		sendto_local_clients_with_capability(CLICAP_CAP_NOTIFY, ":%s CAP * NEW :draft/multiline=%s",
			me.name, capdata_buf);
	}
}

static void
multiline_conf_info(void *data_)
{
	hook_data *data = data_;
	char max_lines_buf[24];
	char max_bytes_buf[24];

	snprintf(max_bytes_buf, sizeof(max_bytes_buf), "%d", max_bytes);
	snprintf(max_lines_buf, sizeof(max_lines_buf), "%d", max_lines);

	sendto_one(data->client, ":%s %d %s :%-30s %-16s [%s]",
		get_id(&me, data->client), RPL_INFO,
		get_id(data->client, data->client),
		"multiline_max_lines",
		max_lines_buf,
		"Maximum number of lines allowed in a multiline batch");

	sendto_one(data->client, ":%s %d %s :%-30s %-16s [%s]",
		get_id(&me, data->client), RPL_INFO,
		get_id(data->client, data->client),
		"multiline_max_bytes",
		max_bytes_buf,
		"Maximum number of bytes allowed in a multiline batch");
}

static void
send_channel_batch(struct Client *client_p, struct Client *source_p,
	struct Channel *chptr, int type, bool opmod,
	struct Batch *batch, size_t n_tags, struct MsgTag tags[])
{
	rb_dlink_node *ptr;
	char origin[USERHOST_REPLYLEN];
	int msgid = -1;

	/* figure out command and normalize to uppercase */
	const char *command = ((struct BatchMessage *)batch->messages.head->data)->msg.cmd;
	enum message_type msgtype = !rb_strcasecmp(command, "PRIVMSG") ? MESSAGE_TYPE_PRIVMSG : MESSAGE_TYPE_NOTICE;
	command = (msgtype == MESSAGE_TYPE_PRIVMSG) ? "PRIVMSG" : "NOTICE";

	for (size_t i = 0; i < n_tags; i++)
	{
		if (!strcmp(tags[i].key, "msgid"))
		{
			msgid = (int)i;
			break;
		}
	}

	snprintf(origin, sizeof(origin), IsPerson(source_p) ? "%s!%s@%s" : "%s",
		source_p->name, source_p->username, source_p->host);

	struct MsgTag inner_tags[] = {
		{ "batch", batch->id, CLICAP_BATCH },
		{ "draft/multiline-concat", NULL, CLICAP_MULTILINE }
	};

	int result = can_send(chptr, source_p, NULL);
	if (result)
	{
		if (result != CAN_SEND_OPV && MyClient(source_p)
			&& !IsOperGeneral(source_p) && !add_channel_target(source_p, chptr))
		{
			sendto_one(source_p, form_str(ERR_TARGCHANGE),
				me.name, source_p->name, chptr->chname);
			return;
		}

		if (result != CAN_SEND_OPV && flood_attack_channel(msgtype, source_p, chptr, chptr->chname))
			return;
	}
	else if (chptr->mode.mode & MODE_OPMODERATE
		&& (!(chptr->mode.mode & MODE_NOPRIVMSGS) || IsMember(source_p, chptr)))
	{
		if (MyClient(source_p) && !IsOperGeneral(source_p) && !add_channel_target(source_p, chptr))
		{
			sendto_one(source_p, form_str(ERR_TARGCHANGE),
				me.name, source_p->name, chptr->chname);
			return;
		}

		if (!flood_attack_channel(msgtype, source_p, chptr, chptr->chname))
		{
			opmod = true;
			type = ONLY_CHANOPS;
		}
	}
	else
	{
		if (msgtype != MESSAGE_TYPE_NOTICE)
			sendto_one_numeric(source_p, ERR_CANNOTSENDTOCHAN,
				form_str(ERR_CANNOTSENDTOCHAN), chptr->chname);
	}

	unsigned int servcaps = NOCAPS;
	if (type != ALL_MEMBERS)
		servcaps |= CAP_CHW;

	const char *status = "";
	if (type & CHFL_VOICE)
		status = "+";
	else if (type & CHFL_CHANOP)
		status = "@";

	sendto_channel_local_with_capability_butone_tags(source_p, type, CLICAP_BATCH | CLICAP_MULTILINE, NOCAPS,
		chptr, n_tags, tags, ":%s BATCH +%s draft/multiline %s%s",
		origin, batch->id, status, chptr->chname);

	if (opmod)
	{
		sendto_match_servs_tags(source_p, "*", CAP_MULTILINE | CAP_STAG | CAP_EOPMOD | servcaps, NOCAPS,
			n_tags, tags, "BATCH +%s draft/multiline =%s",
			batch->id, chptr->chname);

		sendto_match_servs_tags(source_p->servptr, "*", CAP_MULTILINE | CAP_STAG | servcaps, CAP_EOPMOD,
			n_tags, tags, "BATCH +%s draft/multiline @%s",
			batch->id, chptr->chname);
	}
	else
		sendto_match_servs_tags(source_p, "*", CAP_MULTILINE | CAP_STAG | servcaps, NOCAPS,
			n_tags, tags, "BATCH +%s draft/multiline %s%s",
			batch->id, status, chptr->chname);

	RB_DLINK_FOREACH(ptr, batch->messages.head)
	{
		struct BatchMessage *data = ptr->data;
		bool concat = msgbuf_get_tag(&data->msg, "draft/multiline-concat") != NULL;

		sendto_channel_local_with_capability_butone_tags(source_p, type, CLICAP_BATCH | CLICAP_MULTILINE, NOCAPS,
			chptr, concat ? 2 : 1, inner_tags, ":%s %s %s%s :%s",
			origin, command, status, chptr->chname, data->msg.para[2]);

		if (opmod)
		{
			sendto_match_servs_tags(source_p, "*", CAP_MULTILINE | CAP_STAG | CAP_EOPMOD | servcaps, NOCAPS,
				concat ? 2 : 1, inner_tags, "%s =%s :%s",
				command, chptr->chname, data->msg.para[2]);

			sendto_match_servs_tags(source_p->servptr, "*", CAP_MULTILINE | CAP_STAG | servcaps, CAP_EOPMOD,
				concat ? 2 : 1, inner_tags, "NOTICE @%s :<%s:%s> %s",
				chptr->chname, source_p->name, chptr->chname, data->msg.para[2]);
		}
		else
			sendto_match_servs_tags(source_p, "*", CAP_MULTILINE | CAP_STAG | servcaps, NOCAPS,
				concat ? 2 : 1, inner_tags, "%s %s%s :%s",
				command, status, chptr->chname, data->msg.para[2]);

		/* lot of different fallback scenarios for clients who don't support both batch and multiline */
		if (EmptyString(data->msg.para[2]))
			continue;

		sendto_channel_local_with_capability_butone_tags(source_p, type, CLICAP_BATCH, CLICAP_MULTILINE,
			chptr, n_tags, tags, ":%s %s %s%s :%s",
			origin, command, status, chptr->chname, data->msg.para[2]);

		sendto_channel_local_with_capability_butone_tags(source_p, type, NOCAPS, CLICAP_BATCH,
			chptr, n_tags, tags, ":%s %s %s%s :%s",
			origin, command, status, chptr->chname, data->msg.para[2]);

		if (opmod)
		{
			sendto_match_servs_tags(source_p, "*", CAP_EOPMOD | servcaps, CAP_MULTILINE,
				n_tags, tags, "%s =%s :%s",
				command, chptr->chname, data->msg.para[2]);

			sendto_match_servs_tags(source_p->servptr, "*", servcaps, CAP_MULTILINE | CAP_EOPMOD,
				n_tags, tags, "NOTICE @%s :<%s:%s> %s",
				chptr->chname, source_p->name, chptr->chname, data->msg.para[2]);
		}
		else
			sendto_match_servs_tags(source_p, "*", servcaps, CAP_MULTILINE,
				n_tags, tags, "%s %s%s :%s",
				command, status, chptr->chname, data->msg.para[2]);

		/* msgid should only be attached to the first message of the fallback */
		if (msgid > -1)
			tags[msgid].capmask = NOCAPS;
	}

	sendto_channel_local_with_capability_butone(source_p, type, CLICAP_BATCH | CLICAP_MULTILINE, NOCAPS,
		chptr, ":%s BATCH -%s", origin, batch->id);

	sendto_match_servs(source_p, "*", CAP_MULTILINE | CAP_STAG | servcaps, NOCAPS,
		"BATCH -%s", batch->id);

	/* send an echo-message */
	if (MyClient(source_p) && IsClientCapable(source_p, CLICAP_ECHO_MESSAGE))
	{
		if (msgid > -1)
			tags[msgid].capmask = CLICAP_MESSAGE_TAGS;

		/* echo the incoming batch tag rather than our synthesized one */
		inner_tags[0].value = batch->tag;

		sendto_one_tags(source_p, NOCAPS, NOCAPS, n_tags, tags,
			":%s BATCH +%s draft/multiline %s%s",
			origin, batch->tag, status, chptr->chname);

		RB_DLINK_FOREACH(ptr, batch->messages.head)
		{
			struct BatchMessage *data = ptr->data;
			bool concat = msgbuf_get_tag(&data->msg, "draft/multiline-concat") != NULL;

			sendto_one_tags(source_p, NOCAPS, NOCAPS, concat ? 2 : 1, inner_tags,
				":%s %s %s%s :%s",
				origin, command, status, chptr->chname, data->msg.para[2]);
		}

		sendto_one(source_p, ":%s BATCH -%s", origin, batch->tag);
	}
}

static void
send_private_batch(struct Client *client_p, struct Client *source_p,
	struct Client *target_p,
	struct Batch *batch, size_t n_tags, struct MsgTag tags[])
{
	rb_dlink_node *ptr;
	char origin[USERHOST_REPLYLEN];
	int msgid = -1;

	/* figure out command and normalize to uppercase */
	const char *command = ((struct BatchMessage *)batch->messages.head->data)->msg.cmd;
	enum message_type msgtype = !rb_strcasecmp(command, "PRIVMSG") ? MESSAGE_TYPE_PRIVMSG : MESSAGE_TYPE_NOTICE;
	command = (msgtype == MESSAGE_TYPE_PRIVMSG) ? "PRIVMSG" : "NOTICE";

	for (size_t i = 0; i < n_tags; i++)
	{
		if (!strcmp(tags[i].key, "msgid"))
		{
			msgid = (int)i;
			break;
		}
	}

	snprintf(origin, sizeof(origin), IsPerson(source_p) ? "%s!%s@%s" : "%s",
		get_id(source_p, target_p), source_p->username, source_p->host);

	struct MsgTag inner_tags[] = {
		{ "batch", batch->id, CLICAP_BATCH },
		{ "draft/multiline-concat", NULL, CLICAP_MULTILINE },
	};

	if ((MyClient(target_p) && IsClientCapable(target_p, CLICAP_MULTILINE | CLICAP_BATCH))
		|| (!MyClient(target_p) && IsServerCapable(target_p->from, CAP_MULTILINE)))
	{
		sendto_one_tags(target_p, NOCAPS, NOCAPS, n_tags, tags,
			":%s BATCH +%s draft/multiline %s",
			origin, batch->id, target_p->name);

		RB_DLINK_FOREACH(ptr, batch->messages.head)
		{
			struct BatchMessage *data = ptr->data;
			bool concat = msgbuf_get_tag(&data->msg, "draft/multiline-concat") != NULL;
			sendto_one_tags(target_p, NOCAPS, NOCAPS, concat ? 2: 1, inner_tags,
				"%s %s %s :%s",
				origin, command, get_id(target_p, target_p), data->msg.para[2]);
		}

		sendto_one(target_p, ":%s BATCH -%s", origin, batch->id);
	}
	else
	{
		RB_DLINK_FOREACH(ptr, batch->messages.head)
		{
			struct BatchMessage *data = ptr->data;
			if (EmptyString(data->msg.para[2]))
				continue;

			sendto_one_tags(target_p, NOCAPS, NOCAPS,
				n_tags, tags, "%s %s %s :%s",
				origin, command, get_id(target_p, target_p), data->msg.para[2]);

			/* msgid should only be attached to the first message of the fallback */
			if (msgid > -1)
				tags[msgid].capmask = NOCAPS;
		}
	}

	uint64_t CAP_ECHO = capability_get(serv_capindex, "ECHO", NULL);
	uint64_t CAP_ECHOB = capability_get(serv_capindex, "ECHOB", NULL);
	s_assert(CAP_ECHO != 0 && CAP_ECHOB != 0);
	/* we'll get a remote echo if the target server supports ECHO but not MULTILINE **OR** if it supports both ECHOB and MULTILINE */
	if (MyClient(target_p)
		|| (IsServerCapable(target_p->from, CAP_ECHO) && NotServerCapable(target_p->from, CAP_MULTILINE))
		|| IsServerCapable(target_p->from, CAP_ECHOB | CAP_MULTILINE))
	{
		/* echo the incoming batch tag rather than our synthesized one */
		inner_tags[0].value = batch->tag;

		/* send an echo batch here since the target's server won't send one */
		const char *echo_prefix = MyClient(source_p) ? origin : get_id(target_p, source_p);
		uint64_t serv_cap = NOCAPS;
		if (MyClient(source_p))
		{
			if (NotClientCapable(source_p, CLICAP_ECHO_MESSAGE))
				return;

			sendto_one_tags(source_p, NOCAPS, NOCAPS, n_tags, tags,
				":%s BATCH +%s draft/multiline %s",
				echo_prefix, batch->tag, target_p->name);
		}
		else if (!MyClient(source_p))
		{
			char echo_batch_id[16];
			generate_batch_id(echo_batch_id, sizeof(echo_batch_id));
			struct MsgTag echo_batch = { "batch", echo_batch_id, CLICAP_BATCH };

			serv_cap = CAP_ECHOB;
			sendto_one_tags(source_p, serv_cap, NOCAPS, 0, NULL,
				":%s BATCH +%s solanum.chat/echo %s",
				echo_prefix, batch->id, use_id(source_p));

			sendto_one_tags(source_p, serv_cap, NOCAPS, 1, &echo_batch,
				":%s BATCH +%s draft/multiline %s",
				echo_prefix, batch->tag, target_p->name);
		}

		RB_DLINK_FOREACH(ptr, batch->messages.head)
		{
			struct BatchMessage *data = ptr->data;
			bool concat = msgbuf_get_tag(&data->msg, "draft/multiline-concat") != NULL;

			sendto_one_tags(source_p, serv_cap, NOCAPS, concat ? 2 : 1, inner_tags,
				":%s %s %s :%s",
				echo_prefix, command, target_p->name, data->msg.para[2]);
		}

		sendto_one_tags(source_p, serv_cap, NOCAPS, 0, NULL,
			":%s BATCH -%s", echo_prefix, batch->tag);
	}
}

static void
process_multiline(struct Client *client_p, struct Client *source_p, struct Batch *batch, void *unused)
{
	const char *target, *chtarget;
	struct Client *target_p = NULL;
	struct Channel *chptr = NULL;
	int type = ALL_MEMBERS;
	enum message_type msgtype;
	bool opmod = false;
	unsigned int bytes = 0;
	char *combined, *pos;
	const char *command;
	rb_dlink_node *ptr;

	/* do they have the caps? */
	if (MyClient(source_p) && !IsClientCapable(source_p, CLICAP_MULTILINE | CLICAP_BATCH))
	{
		sendto_one(source_p, ":%s FAIL BATCH MULTILINE_INVALID :multiline batches require both the batch and draft/multiline capabilities",
			me.name);
		return;
	}

	/* nested? */
	if (msgbuf_get_tag(&batch->start->msg, "batch") != NULL)
	{
		if (MyClient(source_p))
			sendto_one(source_p, ":%s FAIL BATCH MULTILINE_INVALID :multiline batches cannot be nested inside other batches",
				me.name);
		return;
	}

	/* too many lines? */
	if (MyConnect(source_p) && batch->len > max_lines)
	{
		sendto_one(source_p, ":%s FAIL BATCH MULTILINE_MAX_LINES %d :multiline batch contains too many lines",
			me.name, max_lines);
		return;
	}

	/* get the target from the initial BATCH message */
	if (batch->start->msg.n_para < 4)
	{
		if (MyClient(source_p))
			sendto_one(source_p, ":%s FAIL BATCH MULTILINE_INVALID :multiline batch is missing a target",
				me.name);
		return;
	}

	target = chtarget = batch->start->msg.para[3];
	/* statusmsg/eopmod? */
	if (*target == '@')
	{
		type = ONLY_CHANOPS;
		++chtarget;
	}
	else if (*target == '+')
	{
		type = ONLY_CHANOPSVOICED;
		++chtarget;
	}
	else if (*target == '=' && IsServer(client_p))
	{
		type = ONLY_CHANOPS;
		++chtarget;
		opmod = true;
	}

	if (EmptyString(chtarget))
	{
		sendto_one(source_p, form_str(ERR_NORECIPIENT),
			me.name, source_p->name, "BATCH");
		return;
	}

	/* Unlike m_message we do not support messages sent to username@server or broadcasts with '$' */
	if (IsChanPrefix(*chtarget))
	{
		/* remote servers can't message local channels */
		if (IsServer(client_p) && *chtarget == '&')
			return;

		chptr = find_channel(chtarget);
		if (chptr == NULL)
		{
			sendto_one_numeric(source_p, ERR_NOSUCHNICK,
				form_str(ERR_NOSUCHNICK), chtarget);
			return;
		}

		if (type != ALL_MEMBERS && !opmod)
		{
			struct membership *msptr = find_channel_membership(chptr, source_p);
			if (!IsServer(source_p) && !IsService(source_p) && !is_chanop_voiced(msptr))
			{
				sendto_one(source_p, form_str(ERR_CHANOPRIVSNEEDED),
					get_id(&me, source_p),
					get_id(source_p, source_p),
					target);
				return;
			}
		}
	}
	else if (type != ALL_MEMBERS)
	{
		/* given a statusmsg prefix but what follows isn't a valid channel name */
		sendto_one_numeric(source_p, ERR_NOSUCHNICK,
			form_str(ERR_NOSUCHNICK), target);
		return;
	}
	else
	{
		target_p = MyClient(source_p) ? find_named_client(target) : find_client(target);
		if (target_p == NULL)
		{
			sendto_one_numeric(source_p, ERR_NOSUCHNICK,
				form_str(ERR_NOSUCHNICK), target);
			return;
		}
	}

	/* received from remote server, send it along without repeating validation/hooks */
	if (IsServer(client_p))
	{
		if (target_p != NULL)
			send_private_batch(client_p, source_p, target_p, batch, batch->start->msg.n_tags, batch->start->msg.tags);
		else if (chptr != NULL)
			send_channel_batch(client_p, source_p, chptr, type, opmod, batch, batch->start->msg.n_tags, batch->start->msg.tags);

		return;
	}

	/* determine if this is a PRIVMSG or a NOTICE batch */
	command = ((struct BatchMessage *)batch->messages.head->data)->msg.cmd;
	if (!rb_strcasecmp("PRIVMSG", command))
	{
		msgtype = MESSAGE_TYPE_PRIVMSG;
	}
	else if (!rb_strcasecmp("NOTICE", command))
	{
		msgtype = MESSAGE_TYPE_NOTICE;
	}
	else
	{
		if (MyClient(source_p))
			sendto_one(source_p, ":%s FAIL BATCH MULTILINE_INVALID :multiline batch must only have either PRIVMSG or NOTICE commands",
				me.name);
		return;
	}

	/* If extensions/tag_message_id is loaded, save off the first generated msgid
	 * since calling the privmsg_user/privmsg_channel hooks are what generates msgids and we do that
	 * once for each line of the batch.
	 */
	char msgid[BUFSIZE] = {0};
	const char *generated_msgid;
	bool have_msgid = false;

	/* Validate the rest of the batch before sending any text out.
	 * We need to ensure it stays under max-bytes, commands are valid, targets match,
	 * and then give hooks a chance to filter or modify the message.
	 */
	RB_DLINK_FOREACH(ptr, batch->messages.head)
	{
		struct BatchMessage *data = ptr->data;
		if (rb_strcasecmp(command, data->msg.cmd) != 0)
		{
			sendto_one(source_p, ":%s FAIL BATCH MULTILINE_INVALID :multiline batch must only have either PRIVMSG or NOTICE commands",
				me.name);
			return;
		}

		if (irccmp(target, data->msg.para[1]) != 0)
		{
			sendto_one(source_p, ":%s FAIL BATCH MULTILINE_INVALID_TARGET %s %s :mismatched target within multiline batch",
				me.name, target, data->msg.para[1]);
			return;
		}

		const char *text = data->msg.para[2];
		if (*text == '\x01')
		{
			sendto_one(source_p, ":%s FAIL BATCH MULTILINE_INVALID :multiline batch cannot contain CTCP",
				me.name);
			return;
		}

		bool concat = msgbuf_get_tag(&data->msg, "draft/multiline-concat") != NULL;
		if (EmptyString(text) && concat)
		{
			sendto_one(source_p, ":%s FAIL BATCH MULTILINE_INVALID :cannot send blank line with draft/multiline-concat",
				me.name);
			return;
		}

		bool text_changed = false;
		if (target_p != NULL)
		{
			hook_data_privmsg_user hdata = { msgtype, source_p, target_p, text, 0, &data->msg };
			call_hook(h_privmsg_user, &hdata);
			if (hdata.approved != 0)
			{
				/* assume the hook took care of sending the user any appropriate error message */
				return;
			}

			if (text != hdata.text)
			{
				text = hdata.text;
				text_changed = true;
			}
		}
		else
		{
			hook_data_privmsg_channel hdata = { msgtype, source_p, chptr, text, 0, &data->msg };
			call_hook(h_privmsg_channel, &hdata);
			if (hdata.approved != 0)
			{
				/* assume the hook took care of sending the user any appropriate error message */
				return;
			}

			if (text != hdata.text)
			{
				text = hdata.text;
				text_changed = true;
			}
		}

		if (text_changed)
		{
			/* save off updated text in the original MsgBuf */
			size_t newlen = strlen(text) + 1;
			char *tmp = rb_malloc(data->datalen + newlen);
			memcpy(tmp, data->data, data->datalen);
			strcpy(tmp + data->datalen, text);
			data->msg.para[2] = tmp + data->datalen;
			rb_free(data->data);
			data->data = tmp;
			data->datalen += newlen;
		}

		if (!have_msgid && (generated_msgid = msgbuf_get_tag(&data->msg, "msgid")) != NULL)
		{
			have_msgid = true;
			rb_strlcpy(msgid, generated_msgid, sizeof(msgid));
		}

		size_t len = strlen(text);
		bytes += len;
		/* Add a byte for the newline if this isn't the first message and we lack the multiline-concat tag */
		if (ptr != batch->messages.head && !concat)
			bytes++;

		if (bytes > max_bytes)
		{
			sendto_one(source_p, ":%s FAIL BATCH MULTILINE_MAX_BYTES %d :multiline batch contains too many bytes",
				me.name, max_bytes);
			return;
		}

		/* duplicated here because text may have changed after hooks */
		if (len == 0 && concat)
		{
			sendto_one(source_p, ":%s FAIL BATCH MULTILINE_INVALID :cannot send blank line with draft/multiline-concat",
				me.name);
			return;
		}
	}

	combined = pos = rb_malloc(bytes + 1);
	RB_DLINK_FOREACH(ptr, batch->messages.head)
	{
		struct BatchMessage *data = ptr->data;
		const char *text = data->msg.para[2];
		size_t len = strlen(text);
		bool concat = msgbuf_get_tag(&data->msg, "draft/multiline-concat") != NULL;
		if (ptr != batch->messages.head && !concat)
			*pos++ = '\n';

		strcpy(pos, text);
		pos += len;
	}

	/* at this point we have the full combined text,
	 * make a dummy msgbuf with only tags defined in case hooks want to add more
	 */
	struct MsgBuf batch_msgbuf = { 0 };
	if (have_msgid)
		msgbuf_append_tag(&batch_msgbuf, "msgid", msgid, CLICAP_MESSAGE_TAGS);

	if (target_p != NULL)
	{
		hook_data_privmsg_user hdata = { msgtype, source_p, target_p, combined, 0, &batch_msgbuf };
		call_hook(h_multiline_message_user, &hdata);
		rb_free(combined);
		if (hdata.approved != 0)
		{
			/* assume the hook took care of sending the user any appropriate error message */
			return;
		}

		send_private_batch(client_p, source_p, target_p, batch, batch_msgbuf.n_tags, batch_msgbuf.tags);
	}
	else if (chptr != NULL)
	{
		hook_data_privmsg_channel hdata = { msgtype, source_p, chptr, combined, 0, &batch_msgbuf };
		call_hook(h_multiline_message_channel, &hdata);
		rb_free(combined);
		if (hdata.approved != 0)
		{
			/* assume the hook took care of sending the user any appropriate error message */
			return;
		}

		send_channel_batch(client_p, source_p, chptr, type, opmod, batch, batch_msgbuf.n_tags, batch_msgbuf.tags);
	}
}

static void
multiline_tag_allow(void *data_)
{
	hook_data_message_tag *data = data_;
	if (!strcmp(data->key, "draft/multiline-concat"))
	{
		data->capmask = CLICAP_MULTILINE;
		data->approved = MESSAGE_TAG_ALLOW;
	}
}

DECLARE_MODULE_AV2(multiline, multiline_modinit, multiline_moddeinit, NULL, NULL, multiline_hfn_list, multiline_cap_list, NULL, multiline_desc);
