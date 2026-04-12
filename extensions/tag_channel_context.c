/*
 * Solanum: a slightly advanced ircd
 * tag_channel_context.c: implement the IRCv3 +channel-context client tag
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
#include "channel.h"
#include "client.h"
#include "client_tags.h"
#include "hash.h"
#include "hook.h"
#include "modules.h"
#include "s_serv.h"

static const char tag_ccon_desc[] = "Provides support for the +channel-context client tag.";
static void tag_ccon_allow(void *);

mapi_hfn_list_av1 tag_ccon_hfnlist[] = {
	{ "message_tag", tag_ccon_allow },
	{ NULL, NULL }
};

static int
modinit(void)
{
	add_client_tag("channel-context");
	return 0;
}

static void
moddeinit(void)
{
	remove_client_tag("channel-context");
}

static void
tag_ccon_allow(void *data_)
{
	hook_data_message_tag *data = data_;

	if (MyClient(data->source))
	{
		if (NotClientCapable(data->source, CLICAP_MESSAGE_TAGS))
			return;

		/* +client-context is only allowed on PRIVMSG and NOTICE */
		if (strcasecmp(data->message->cmd, "PRIVMSG") != 0 && strcasecmp(data->message->cmd, "NOTICE") != 0)
			return;

		/* sending +channel-context to a local channel would require that the sender and recipient(s) are on the
		 * same server. This is annoying to test as we haven't parsed the message target yet,
		 * so just don't allow sending the tag to local channels at all.
		 */
		if (EmptyString(data->value) || *data->value == '&')
			return;

		/* valid channel? */
		struct Channel *chptr = find_channel(data->value);
		if (chptr == NULL)
			return;

		/* sender not on the channel or can't send messages to the channel? */
		struct membership *msptr = find_channel_membership(chptr, data->source);
		if (msptr == NULL || can_send(chptr, data->source, msptr) == CAN_SEND_NO)
			return;
	}

	data->approved = MESSAGE_TAG_ALLOW;
	data->capmask = CLICAP_MESSAGE_TAGS;
}

DECLARE_MODULE_AV2(tag_channel_context, modinit, moddeinit, NULL, NULL, tag_ccon_hfnlist, NULL, NULL, tag_ccon_desc);
