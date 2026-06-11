/*
 *  umode_bot.c: Bot self-identification
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
#include "hook.h"
#include "ircd.h"
#include "messages.h"
#include "modules.h"
#include "numeric.h"
#include "ircd.h"
#include "logger.h"
#include "send.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_serv.h"
#include "s_user.h"
#include "supported.h"

static const char umode_bot_desc[] =
	"Adds user mode +B which marks a client as a bot";

static unsigned int umode;

#define IsBot(client) ((client)->umodes & umode)

static int
modinit(void)
{
	umode = find_umode_slot();
	if (!umode)
	{
		ierror("umode_bot: unable to allocate umode slot for +B, unloading module");
		return -1;
	}
	user_modes['B'] = umode;

	construct_umodebuf();
	/* Don't use isupport_umode here the value is also the WHO flag for bots. */
	add_isupport("BOT", isupport_string, "B");
	return 0;
}

static void
moddeinit(void)
{
	user_modes['B'] = 0;
	construct_umodebuf();
	delete_isupport("BOT");
}

static void
umode_bot_apply_tag(void *data_)
{
	hook_data_outbound_msgbuf *data = data_;
	struct MsgBuf *msgbuf = data->msgbuf;

	if (data->source != NULL && IsBot(data->source))
		msgbuf_append_tag(msgbuf, "bot", NULL, CLICAP_MESSAGE_TAGS);
}

static void
umode_bot_whois(void *data_)
{
	hook_data_client *data = data_;
	if (!IsBot(data->target))
		return;

	sendto_one_numeric(data->client, RPL_WHOISBOT, form_str(RPL_WHOISBOT), data->target->name);
}

static mapi_hfn_list_av1 umode_bot_hfnlist[] = {
	{ "outbound_msgbuf", umode_bot_apply_tag },
	{ "doing_whois", umode_bot_whois },
	{ "doing_whois_global", umode_bot_whois },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(umode_bot, modinit, moddeinit, NULL, NULL, umode_bot_hfnlist, NULL, NULL, umode_bot_desc);
