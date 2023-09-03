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
#include "ircd.h"

static const char extb_desc[] = "Gateway name ($w) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_gateway(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV2(extb_gateway, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	extban_table['w'] = eb_gateway;

	return 0;
}

static void
_moddeinit(void)
{
	extban_table['w'] = NULL;
}

static int eb_gateway(const char *data, struct Client *client_p,
		struct Channel *chptr, long mode_type)
{
	/* $w by itself will match all gateway users */
	if (data == NULL)
		return EmptyString(client_p->gateway) ? EXTBAN_NOMATCH : EXTBAN_MATCH;
	return match(data, client_p->gateway) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}
