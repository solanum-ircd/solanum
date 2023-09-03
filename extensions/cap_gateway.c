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

#include <stdinc.h>
#include <modules.h>
#include <capability.h>
#include <s_serv.h>
#include <s_newconf.h>
#include <client.h>
#include <msgbuf.h>

static char cap_gateway_desc[] = "Provides the solanum.chat/gateway capability";

static void cap_gateway_outbound_msgbuf(void *);

static unsigned CLICAP_GATEWAY;

mapi_cap_list_av2 cap_gateway_caps[] = {
	{ MAPI_CAP_CLIENT, "solanum.chat/gateway", NULL, &CLICAP_GATEWAY },
	{ 0, NULL, NULL, NULL }
};

mapi_hfn_list_av1 cap_gateway_hfnlist[] = {
	{ "outbound_msgbuf", cap_gateway_outbound_msgbuf, HOOK_NORMAL },
	{ NULL, NULL, 0 }
};


static void
cap_gateway_outbound_msgbuf(void *data_)
{
	hook_data *data = data_;
	struct MsgBuf *msgbuf = data->arg1;

	if (data->client == NULL || !IsPerson(data->client))
		return;

	if (!EmptyString(data->client->gateway))
		msgbuf_append_tag(msgbuf, "solanum.chat/gateway", data->client->gateway, CLICAP_GATEWAY);
}

DECLARE_MODULE_AV2(cap_gateway, NULL, NULL, NULL, NULL, cap_gateway_hfnlist, cap_gateway_caps, NULL, cap_gateway_desc);
