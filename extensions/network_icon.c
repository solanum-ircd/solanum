/*
 *  Solanum: a slightly advanced ircd
 *  network_icon.c: Advertise a network icon.
 *
 *  Copyright (C) 2026 internet-catte
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
#include "newconf.h"
#include "supported.h"

static const char network_icon_desc[] = "Provides the draft/ICON ISUPPORT token for clients that support showing a network icon";

static int modinit(void);
static void moddeinit(void);
static void network_icon_init_conf(void *data);
static void network_icon_conf_set_url(void *data);

static char *network_icon_url = NULL;

mapi_hfn_list_av1 network_icon_hfnlist[] = {
	{ "conf_read_start", network_icon_init_conf },
};

DECLARE_MODULE_AV2(network_icon, modinit, moddeinit, NULL, NULL, network_icon_hfnlist, NULL, NULL, network_icon_desc);

static int
modinit(void)
{
	add_conf_item("general", "network_icon_url", CF_QSTRING, network_icon_conf_set_url);
	add_isupport("draft/ICON", isupport_stringptr, &network_icon_url);
	return 0;
}

static void
moddeinit(void)
{
	delete_isupport("draft/ICON");
	rb_free(network_icon_url);
	remove_conf_item("general", "network_icon_url");
}

static void
network_icon_init_conf(void *data)
{
	rb_free(network_icon_url);
	network_icon_url = NULL;
}

static void
network_icon_conf_set_url(void *data)
{
	rb_free(network_icon_url);
	network_icon_url = rb_strdup(data);
}
