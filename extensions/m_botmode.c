/*
 *  Copyright (C) 2021 David Schultz <me@zpld.me>
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
 */

#include "stdinc.h"
#include "capability.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "send.h"
#include "logger.h"
#include "modules.h"
#include "msgbuf.h"
#include "numeric.h"
#include "supported.h"
#include "s_conf.h"
#include "s_serv.h"
#include "s_user.h"
#include "s_newconf.h"

static char botmode_desc[] = "Provides support for the IRCv3 draft/bot spec";

static void h_bm_outbound_msgbuf(void *);
static void h_bm_whois(void *);

static unsigned CLICAP_BOT;

mapi_cap_list_av2 botmode_caps[] = {
	{ MAPI_CAP_CLIENT, "draft/bot", NULL, &CLICAP_BOT },
	{ 0, NULL, NULL, NULL},
};

mapi_hfn_list_av1 botmode_hfnlist[] = {
	{ "outbound_msgbuf", h_bm_outbound_msgbuf, HOOK_NORMAL },
	{ "doing_whois_global", h_bm_whois, HOOK_MONITOR },
	{ "doing_whois", h_bm_whois, HOOK_MONITOR },
	{ NULL, NULL, 0 }
};

static void
h_bm_outbound_msgbuf(void *data_)
{
	hook_data *data = data_;
	struct MsgBuf *msgbuf = data->arg1;

	if (data->client->umodes & user_modes['B'])
	{
		msgbuf_append_tag(msgbuf, "draft/bot", NULL, CLICAP_BOT);
	}
}

static void
h_bm_whois(void *data_)
{
	hook_data_client *data = data_;
	if (data->target->umodes & user_modes['B'])
	{
		sendto_one_numeric(data->client, RPL_WHOISBOT,
			form_str(RPL_WHOISBOT), data->target->name, ServerInfo.network_name);
	}
}

static int
_modinit(void)
{
	user_modes['B'] = find_umode_slot();
	construct_umodebuf();
	if (!user_modes['B'])
	{
		ierror("m_botmode: unable to allocate usermode slot for +B, unloading extension");
		return -1;
	}
	add_isupport("BOT", isupport_umode, "B");
	return 0;
}

static void
_moddeinit(void)
{
	user_modes['B'] = 0;
	construct_umodebuf();
	delete_isupport("BOT");
}

DECLARE_MODULE_AV2(botmode, _modinit, _moddeinit, NULL, NULL, botmode_hfnlist, botmode_caps, NULL, botmode_desc);
