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
static void tag_reply_allow(void *);

mapi_hfn_list_av1 tag_reply_hfnlist[] = {
	{ "message_tag", tag_reply_allow },
	{ NULL, NULL }
};

static int
modinit(void)
{
	add_client_tag("reply");
	return 0;
}

static void
moddeinit(void)
{
	remove_client_tag("reply");
}

static void
tag_reply_allow(void *data_)
{
	hook_data_message_tag *data = data_;
	const char *target = data->message->para[1];

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

DECLARE_MODULE_AV2(tag_reply, modinit, moddeinit, NULL, NULL, tag_reply_hfnlist, NULL, NULL, tag_reply_desc);
