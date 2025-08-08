/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_motd.c: Shows the current message of the day.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "numeric.h"
#include "hook.h"
#include "msg.h"
#include "s_serv.h"		/* hunt_server */
#include "parse.h"
#include "modules.h"
#include "s_conf.h"
#include "cache.h"
#include "ratelimit.h"

static const char motd_desc[] = "Provides the MOTD command to view the Message of the Day";

static void m_motd(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void mo_motd(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message motd_msgtab = {
	"MOTD", 0, 0, 0, 0,
	{mg_unreg, {m_motd, 0}, {mo_motd, 0}, mg_ignore, mg_ignore, {mo_motd, 0}}
};

int doing_motd_hook;

mapi_clist_av1 motd_clist[] = { &motd_msgtab, NULL };
mapi_hlist_av1 motd_hlist[] = {
	{ "doing_motd",	&doing_motd_hook },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(motd, NULL, NULL, motd_clist, motd_hlist, NULL, NULL, NULL, motd_desc);

/*
** m_motd
**      parv[1] = servername
*/
static void
m_motd(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0;

	if (parc < 2) {
		/* do nothing */
	} else if ((last_used + ConfigFileEntry.pace_wait) > rb_current_time() || !ratelimit_client(source_p, 6)) {
		/* safe enough to give this on a local connect only */
		sendto_one(source_p, form_str(RPL_LOAD2HI),
			   me.name, source_p->name, "MOTD");
		sendto_one(source_p, form_str(RPL_ENDOFMOTD),
			   me.name, source_p->name);
		return;
	} else {
		last_used = rb_current_time();
	}

	if(hunt_server(client_p, source_p, ":%s MOTD :%s", 1, parc, parv) != HUNTED_ISME)
		return;

	send_user_motd(source_p);
}

/*
** mo_motd
**      parv[1] = servername
*/
static void
mo_motd(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(hunt_server(client_p, source_p, ":%s MOTD :%s", 1, parc, parv) != HUNTED_ISME)
		return;

	send_user_motd(source_p);
}
