/*
 *  botmode.c: Bot self-identification and opt-out channel mode
 *
 *  Copyright (C) 2026 TheDaemoness
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id$
 */

#include "stdinc.h"
#include "channel.h"
#include "client.h"
#include "chmode.h"
#include "hook.h"
#include "ircd.h"
#include "messages.h"
#include "modules.h"
#include "numeric.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_serv.h"
#include "s_user.h"
#include "supported.h"

static const char botmode_desc[] =
	"Implements bot mode and adds a channel mode to prohibit bots from joining or speaking";

static unsigned int chmode, umode;

#define IsBot(client) ((client)->umodes & umode)

static int
modinit(void)
{
	if (user_modes['B'])
		return -1;
	umode = user_modes['B'] = find_umode_slot();
	if (!umode)
		return -1;

	chmode = cflag_add('B', chm_simple);
	if (!chmode)
	{
		user_modes['B'] = 0;
		return -1;
	}

	construct_umodebuf();
	add_isupport("BOT", isupport_string, "B");
	return 0;
}

static void
moddeinit(void)
{
	user_modes['B'] = 0;
	construct_umodebuf();
	delete_isupport("BOT");
	cflag_orphan('B');
}

static void
botmode_can_send(void *data_)
{
	hook_data_channel *data = data_;
	struct membership *msptr;

	/* If message is already blocked, defer. */
	if (data->approved == CAN_SEND_NO)
		return;

	if (!IsBot(data->client))
		return;

	if (!(data->chptr->mode.mode & chmode))
		return;

	/* Allow bots with oper:always_message to bypass this mode. */
	if (HasPrivilege(data->client, "oper:always_message"))
		return;

	msptr = find_channel_membership(data->chptr, data->client);
	if (is_chanop_voiced(msptr))
		return;

	/* If we're here, umode AND cmode +B are set, and the client is not exempt for any reason. */
	sendto_one_numeric(data->client, ERR_CANNOTSENDTOCHAN,
			"%s :Cannot send to channel (+B) - bots must be opped/voiced",
			data->chptr->chname);
	data->approved = CAN_SEND_NO;
}

static void
botmode_can_join(void *data_)
{
	hook_data_channel *data = data_;
	struct Client *client = data->client;
	struct Channel *chptr = data->chptr;
	rb_dlink_node *invite = NULL;
	struct Ban *invex = NULL;
	struct matchset ms;
	rb_dlink_node *ptr;

	/* If join is already blocked, defer. */
	if (data->approved)
		return;

	if (!IsBot(client))
		return;

	if (!(chptr->mode.mode & chmode))
		return;

	/* Allow oper bots to bypass this mode. */
	if (IsOper(data->client))
		return;

	/* Check for invites. */
	RB_DLINK_FOREACH(invite, client->user->invited.head)
	{
		if (invite->data == chptr)
			return;
	}

	/* Check for invexes. */
	if (ConfigChannel.use_invex)
	{
		RB_DLINK_FOREACH(ptr, chptr->invexlist.head)
		{
			invex = ptr->data;
			if (matches_mask(&ms, invex->banstr) ||
					match_extban(invex->banstr, client, chptr, CHFL_INVEX))
				return;
		}
	}

	/* If we're here, umode AND cmode +B are set, and the client is not exempt for any reason. */
	sendto_one_numeric(data->client, ERR_INVITEONLYCHAN,
			"%s :Cannot join channel (+B) - bots must be invited",
			chptr->chname);
	data->approved = ERR_CUSTOM;
}

static void
botmode_apply_tag(void *data_)
{
	hook_data *data = data_;
	struct MsgBuf *msgbuf = data->arg1;

	if (data->client != NULL && IsBot(data->client) && *data->client->user->suser)
		msgbuf_append_tag(msgbuf, "bot", NULL, CLICAP_MESSAGE_TAGS);
}

static void
botmode_whois(void *data_)
{
	hook_data_client *data = data_;
	if(!IsBot(data->target))
		return;

	sendto_one_numeric(data->client, RPL_WHOISBOT, form_str(RPL_WHOISBOT), data->target->name);
}

static void
botmode_change(void *data_)
{
	hook_data_umode_changed *data = data_;

	/* Check if we are in at least one channel. */
	if (data->client->user->channel.head == NULL)
		return;

	if (IsBot(data->client) && !(data->oldumodes & umode))
	{
		/* Attempted to set botmode while in a channel. TODO: Send error. */
		data->client->umodes &= ~umode;
	}
	else if (!IsBot(data->client) && (data->oldumodes & umode))
	{
		/* Attempted to unset botmode while in a channel. TODO: Send error. */
		data->client->umodes |= umode;
	}
}

static mapi_hfn_list_av1 botmode_hfnlist[] = {
	{ "can_send", botmode_can_send },
	{ "can_join", botmode_can_join },
	{ "outbound_msgbuf", botmode_apply_tag },
	{ "doing_whois", botmode_whois },
	{ "doing_whois_global", botmode_whois },
	{ "umode_change", botmode_change },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(botmode, modinit, moddeinit, NULL, NULL, botmode_hfnlist, NULL, NULL, botmode_desc);
