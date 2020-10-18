/*
 *  Copyright (C) 2020 Ed Kellett
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

static char cap_realhost_desc[] = "Provides the solanum.chat/realhost oper-only capability";

static bool cap_realhost_visible(struct Client *);
static void cap_realhost_outbound_msgbuf(void *);
static void cap_realhost_umode_changed(void *);

static unsigned CLICAP_REALHOST;

static struct ClientCapability cap_realhost = {
	.visible = cap_realhost_visible,
};

mapi_cap_list_av2 cap_realhost_caps[] = {
	{ MAPI_CAP_CLIENT, "solanum.chat/realhost", &cap_realhost, &CLICAP_REALHOST },
	{ 0, NULL, NULL, NULL },
};

mapi_hfn_list_av1 cap_realhost_hfnlist[] = {
	{ "outbound_msgbuf", cap_realhost_outbound_msgbuf, HOOK_NORMAL },
	{ "umode_changed", cap_realhost_umode_changed, HOOK_MONITOR },
	{ NULL, NULL, 0 },
};

static bool
cap_realhost_visible(struct Client *client)
{
	return HasPrivilege(client, "cap:realhost");
}

static void
cap_realhost_outbound_msgbuf(void *data_)
{
	hook_data *data = data_;
	struct MsgBuf *msgbuf = data->arg1;

	if (data->client == NULL)
		return;

	if (!IsIPSpoof(data->client) && !EmptyString(data->client->sockhost) && strcmp(data->client->sockhost, "0"))
		msgbuf_append_tag(msgbuf, "solanum.chat/ip", data->client->sockhost, CLICAP_REALHOST);

	if (!EmptyString(data->client->orighost))
		msgbuf_append_tag(msgbuf, "solanum.chat/realhost", data->client->orighost, CLICAP_REALHOST);
}

static void
cap_realhost_umode_changed(void *data_)
{
	hook_data_umode_changed *data = data_;

	if (!MyClient(data->client))
		return;

	if (!cap_realhost_visible(data->client))
		data->client->localClient->caps &= ~CLICAP_REALHOST;
}

DECLARE_MODULE_AV2(cap_realhost, NULL, NULL, NULL, NULL, cap_realhost_hfnlist, cap_realhost_caps, NULL, cap_realhost_desc);
