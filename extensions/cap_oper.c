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
#include <s_conf.h>
#include <s_serv.h>
#include <s_newconf.h>
#include <client.h>
#include <msgbuf.h>

static char cap_oper_desc[] = "Provides the solanum.chat/oper capability";

static bool cap_oper_oper_visible(struct Client *);
static void cap_oper_outbound_msgbuf(void *);
static void cap_oper_umode_changed(void *);
static void cap_oper_cap_change(void *);

static unsigned CLICAP_OPER;
static unsigned CLICAP_OPER_AUSPEX;
static unsigned CLICAP_OPER_JUSTOPER;
static unsigned CLICAP_OPER_NORMAL;

static struct ClientCapability capdata_oper_oper = {
	.visible = cap_oper_oper_visible,
};

mapi_cap_list_av2 cap_oper_caps[] = {
	{ MAPI_CAP_CLIENT, "solanum.chat/oper", NULL, &CLICAP_OPER },
	{ MAPI_CAP_CLIENT, "?oper_auspex", &capdata_oper_oper, &CLICAP_OPER_AUSPEX },
	{ MAPI_CAP_CLIENT, "?oper_justoper", &capdata_oper_oper, &CLICAP_OPER_JUSTOPER },
	{ MAPI_CAP_CLIENT, "?oper_normal", &capdata_oper_oper, &CLICAP_OPER_NORMAL },
	{ 0, NULL, NULL, NULL },
};

mapi_hfn_list_av1 cap_oper_hfnlist[] = {
	{ "outbound_msgbuf", cap_oper_outbound_msgbuf, HOOK_NORMAL },
	{ "umode_changed", cap_oper_umode_changed, HOOK_MONITOR },
	{ "cap_change", cap_oper_cap_change, HOOK_MONITOR },
	{ NULL, NULL, 0 },
};

static bool
cap_oper_oper_visible(struct Client *client)
{
	return false;
}

static void
cap_oper_outbound_msgbuf(void *data_)
{
	hook_data *data = data_;
	struct MsgBuf *msgbuf = data->arg1;

	if (data->client == NULL || !IsPerson(data->client))
		return;

	if (IsOper(data->client))
	{
		/* send all oper data to auspex */
		msgbuf_append_tag(msgbuf, "solanum.chat/oper", data->client->user->opername, CLICAP_OPER_AUSPEX);
		if (HasPrivilege(data->client, "oper:hidden") || ConfigFileEntry.hide_opers)
			/* these people aren't allowed to see hidden opers */
			return;
		msgbuf_append_tag(msgbuf, "solanum.chat/oper", data->client->user->opername, CLICAP_OPER_JUSTOPER);
		msgbuf_append_tag(msgbuf, "solanum.chat/oper", NULL, CLICAP_OPER_NORMAL);
	}
}

static inline void
update_clicap_oper(struct Client *client)
{
	/* clear out old caps */
	client->localClient->caps &= ~CLICAP_OPER_AUSPEX;
	client->localClient->caps &= ~CLICAP_OPER_JUSTOPER;
	client->localClient->caps &= ~CLICAP_OPER_NORMAL;

	if (client->localClient->caps & CLICAP_OPER && HasPrivilege(client, "auspex:oper"))
	{
		/* if the client is an oper with auspex, let them see everything */
		client->localClient->caps |= CLICAP_OPER_AUSPEX;
	}
	else if (client->localClient->caps & CLICAP_OPER && IsOper(client))
	{
		/* if the client is an oper, let them see other opers */
		client->localClient->caps |= CLICAP_OPER_JUSTOPER;
	}
	else if (client->localClient->caps & CLICAP_OPER)
	{
		/* if the client is a normal user, let them see opers
		   provided that server wide oper hiding is not enabled */
		client->localClient->caps |= CLICAP_OPER_NORMAL;
	}
}

static void
cap_oper_umode_changed(void *data_)
{
	hook_data_umode_changed *data = data_;

	if (!MyClient(data->client))
		return;

	update_clicap_oper(data->client);
}

static void
cap_oper_cap_change(void *data_)
{
	hook_data_cap_change *data = data_;

	update_clicap_oper(data->client);
}

static int
modinit(void)
{
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, lclient_list.head)
	{
		struct Client *client = ptr->data;

		update_clicap_oper(client);
	}

	return 0;
}

DECLARE_MODULE_AV2(cap_oper, modinit, NULL, NULL, NULL, cap_oper_hfnlist, cap_oper_caps, NULL, cap_oper_desc);
