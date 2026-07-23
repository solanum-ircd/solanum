/*
 *  restrict_botmode.c: Freeze bot mode while joined to channels
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
#include "modules.h"
#include "ircd.h"
#include "logger.h"
#include "send.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_serv.h"
#include "s_user.h"
#include <complex.h>

static const char restrict_botmode_desc[] =
	"Prevents changing user mode B while joined to channels";

#define HasMode(data, umode) ((data)->client->umodes & (umode))
#define HadMode(data, umode) ((data)->oldumodes & (umode))

static void
restrict_botmode(void *data_)
{
	hook_data_umode_changed *data = data_;
	unsigned int umode = user_modes['B'];
	/* No need to check if umode is 0; if it is for some reason, both macros will evaluate to false. */

	/* Check if we are in at least one channel. */
	if (data->client->user->channel.length == 0)
		return;

	if (HasMode(data, umode) && !HadMode(data, umode))
	{
		/* TODO: Send error. */
		data->client->umodes &= ~umode;
	}
	else if (!HasMode(data, umode) && HadMode(data, umode))
	{
		/* TODO: Send error. */
		data->client->umodes |= umode;
	}
}

static mapi_hfn_list_av1 restrict_botmode_hfnlist[] = {
	{ "umode_changed", restrict_botmode },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(restrict_botmode, NULL, NULL, NULL, NULL, restrict_botmode_hfnlist, NULL, NULL, restrict_botmode_desc);
