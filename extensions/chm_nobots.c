/*
 *  chm_nobots.c: Prevent bots from joining/sending unless invited/voiced
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
#include "modules.h"
#include "numeric.h"
#include "ircd.h"
#include "logger.h"
#include "send.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_serv.h"
#include "s_user.h"

static const char chm_nobots_desc[] =
	"Adds channel mode +B which requires users with bot mode to be invited/voiced to join/speak";

static unsigned int chmode;

#define IsBot(client) ((client)->umodes & user_modes['B'])

static int
modinit(void)
{
	chmode = cflag_add('B', chm_simple);
	if (!chmode)
	{
		ierror("chm_nobots: unable to allocate cmode slot for +B, unloading module");
		return -1;
	}

	return 0;
}

static void
moddeinit(void)
{
	cflag_orphan('B');
}

static void
chm_nobots_can_send(void *data_)
{
	hook_data_channel_approval *data = data_;

	/* If message is already blocked or the sender is opped/voiced, exit early.*/
	if (data->approved != CAN_SEND_NONOP)
		return;

	if (!IsBot(data->client))
		return;

	if (!(data->chptr->mode.mode & chmode))
		return;

	/* Allow bots with oper:always_message to bypass this mode. */
	if (HasPrivilege(data->client, "oper:always_message"))
		return;

	/* If we're here, umode AND cmode +B are set, and the client is not exempt for any reason. */
	sendto_one_numeric(data->client, ERR_CANNOTSENDTOCHAN,
			"%s :Cannot send to channel (+B) - bots must be opped/voiced",
			data->chptr->chname);
	data->approved = CAN_SEND_NO;
}

static void
chm_nobots_can_join(void *data_)
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
		matchset_for_client(client, &ms);
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

static mapi_hfn_list_av1 chm_nobots_hfnlist[] = {
	{ "can_send", chm_nobots_can_send },
	{ "can_join", chm_nobots_can_join },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(chm_nobots, modinit, moddeinit, NULL, NULL, chm_nobots_hfnlist, NULL, NULL, chm_nobots_desc);
