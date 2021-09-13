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
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "logger.h"
#include "send.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_newconf.h"

static const char hide_desc[] = "Provides user mode +I to hide a user's idle time";

static void h_huc_doing_idle_time_hook(void *);

mapi_hfn_list_av1 huc_hfnlist[] = {
	{ "doing_whois_show_idle", h_huc_doing_idle_time_hook },
	{ "doing_trace_show_idle", h_huc_doing_idle_time_hook },
	{ "doing_stats_show_idle", h_huc_doing_idle_time_hook },
	{ "doing_who_show_idle", h_huc_doing_idle_time_hook },
	{ NULL, NULL }
};

static void
h_huc_doing_idle_time_hook(void *data_)
{
	hook_data_client_approval *data = data_;

	if (data->approved == 0)
		return;

	if (data->target->umodes & user_modes['I'])
	{
		if ((data->client != data->target) && !HasPrivilege(data->client, "auspex:usertimes"))
			data->approved = 0;
		else if (HasPrivilege(data->client, "auspex:usertimes"))
			data->approved = 2;
	}
}

static int
_modinit(void)
{
	user_modes['I'] = find_umode_slot();
	construct_umodebuf();
	if (!user_modes['I'])
	{
		ierror("umode_hide_idle_time: unable to allocate usermode slot for +I, unloading extension");
		return -1;
	}
	return 0;
}

static void
_moddeinit(void)
{
	user_modes['I'] = 0;
	construct_umodebuf();
}

DECLARE_MODULE_AV2(hide_idle_time, _modinit, _moddeinit, NULL, NULL, huc_hfnlist, NULL, NULL, hide_desc);
