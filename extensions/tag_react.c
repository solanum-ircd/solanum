/*
 * Solanum: a slightly advanced ircd
 * tag_react.c: implement the IRCv3 +draft/react and +draft/unreact client tags
 *
 * Copyright (c) 2025 Ryan Schmidt <skizzerz@skizzerz.net>
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
#include "parse.h"
#include "inline/stringops.h"

#include <unicode/uchar.h>
#include <unicode/ustring.h>

static const char tag_react_desc[] = "Provides support for the +draft/react and +draft/unreact client tags.";
static void tag_react_allow(void *);
static void tag_react_outgoing(void *);

mapi_hfn_list_av1 tag_react_hfnlist[] = {
	{ "message_tag", tag_react_allow },
	{ "outbound_msgbuf", tag_react_outgoing },
	{ NULL, NULL }
};

static int
modinit(void)
{
	add_client_tag("draft/react");
	add_client_tag("draft/unreact");
	return 0;
}

static void
moddeinit(void)
{
	remove_client_tag("draft/react");
	remove_client_tag("draft/unreact");
}

static void
tag_react_allow(void *data_)
{
	hook_data_message_tag *data = data_;
	if (IsClient(data->client) && NotClientCapable(data->client, CLICAP_MESSAGE_TAGS))
		return;

	if (strcmp("+draft/react", data->key) != 0 && strcmp("+draft/unreact", data->key) != 0)
		return;

	/* Reaction requires a value */
	if (EmptyString(data->value))
		return;

	/* Don't filter server-sent messages */
	if (IsServer(data->client))
	{
		data->approved = MESSAGE_TAG_ALLOW;
		data->capmask = CLICAP_MESSAGE_TAGS;
		return;
	}

	UChar value[BUFSIZE];
	int32_t len = 0;

	/* destCapacity is in elements, not bytes. Subtract 1 to ensure that the trailing NULL can be written */
	UErrorCode err = U_ZERO_ERROR;
	u_strFromUTF8(value, BUFSIZE - 1, &len, data->value, -1, &err);
	value[BUFSIZE - 1] = 0;

	/* invalid UTF-8 or too long */
	if (err != U_ZERO_ERROR)
		return;

	/* Validate that the string is a single emoji (limited to those recommended for general interchange).
	 * From testing, this returns false if the string contains multiple emoji or contains any non-emoji characters
	 * but successfully returns true for complex emoji consisting of multiple combining marks and characters
	 */
	if (u_stringHasBinaryProperty(value, len, UCHAR_RGI_EMOJI))
	{
		data->approved = MESSAGE_TAG_ALLOW;
		data->capmask = CLICAP_MESSAGE_TAGS;
	}
}

static void
tag_react_outgoing(void *data_)
{
	hook_data_outbound_msgbuf *data = data_;
	struct MsgBuf *msgbuf = data->msgbuf;

	bool has_reply = msgbuf_get_tag(msgbuf, "+reply") != NULL;
	bool has_react = msgbuf_get_tag(msgbuf, "+draft/react") != NULL;

	for (int i = 0; i < msgbuf->n_tags; i++)
	{
		const char *key = msgbuf->tags[i].key;
		/* strip (un)react from outgoing messages that lack a reply */
		if (!has_reply && (!strcmp(key, "+draft/react") || !strcmp(key, "+draft/unreact")))
			msgbuf->tags[i].capmask = NOCAPS;
		/* strip unreact if we already have a react (messages must not contain both) */
		else if (has_react && !strcmp(key, "+draft/unreact"))
			msgbuf->tags[i].capmask = NOCAPS;
	}
}

DECLARE_MODULE_AV2(tag_react, modinit, moddeinit, NULL, NULL, tag_react_hfnlist, NULL, NULL, tag_react_desc);
