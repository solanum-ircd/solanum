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
#include <hook.h>
#include <match.h>
#include <channel.h>

static const char match_gatewayip_desc[] = "Consider .../ip.X.X.X.X hostnames in channel IP ban matching";

static void hook_match_client(void *);

mapi_hfn_list_av1 match_gatewayip_hfnlist[] = {
	{ "match_client", hook_match_client },
	{ NULL, NULL }
};

void hook_match_client(void *data_)
{
	hook_data_match_client *data = data_;
	const char *p;

	if (data->mode_type != CHFL_BAN && data->mode_type != CHFL_QUIET)
		return;

	if (!IsDynSpoof(data->client))
		return;

	p = strstr(data->client->host, "/ip.");
	if (p == NULL)
		return;

	matchset_append_ip(data->ms, "%s!%s@%s", data->client->name, data->client->username, p + 4);
}

DECLARE_MODULE_AV2(match_gatewayip, NULL, NULL, NULL, NULL, match_gatewayip_hfnlist, NULL, NULL, match_gatewayip_desc);
