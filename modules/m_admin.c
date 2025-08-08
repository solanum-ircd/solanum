/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_admin.c: Sends administrative information to a user.
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
#include "numeric.h"
#include "s_conf.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "hook.h"
#include "modules.h"

const char admin_desc[] =
	"Provides the ADMIN command to show server administrator information";

static void m_admin(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void mr_admin(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void ms_admin(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void do_admin(struct Client *source_p);

struct Message admin_msgtab = {
	"ADMIN", 0, 0, 0, 0,
	{{mr_admin, 0}, {m_admin, 0}, {ms_admin, 0}, mg_ignore, mg_ignore, {ms_admin, 0}}
};

int doing_admin_hook;

mapi_clist_av1 admin_clist[] = { &admin_msgtab, NULL };
mapi_hlist_av1 admin_hlist[] = {
	{ "doing_admin",	&doing_admin_hook },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(admin, NULL, NULL, admin_clist, admin_hlist, NULL, NULL, NULL, admin_desc);

/*
 * mr_admin - ADMIN command handler
 *      parv[1] = servername
 */
static void
mr_admin(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0L;

	if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
	{
		sendto_one(source_p, form_str(RPL_LOAD2HI),
			   me.name,
			   EmptyString(source_p->name) ? "*" : source_p->name,
			   "ADMIN");
		return;
	}
	else
		last_used = rb_current_time();

	do_admin(source_p);
}

/*
 * m_admin - ADMIN command handler
 *      parv[1] = servername
 */
static void
m_admin(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0L;

	if(parc > 1)
	{
		if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
		{
			sendto_one(source_p, form_str(RPL_LOAD2HI),
				   me.name, source_p->name, "ADMIN");
			return;
		}
		else
			last_used = rb_current_time();

		if(hunt_server(client_p, source_p, ":%s ADMIN :%s", 1, parc, parv) != HUNTED_ISME)
			return;
	}

	do_admin(source_p);
}


/*
 * ms_admin - ADMIN command handler, used for OPERS as well
 *      parv[1] = servername
 */
static void
ms_admin(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(hunt_server(client_p, source_p, ":%s ADMIN :%s", 1, parc, parv) != HUNTED_ISME)
		return;

	do_admin(source_p);
}


/*
 * do_admin
 *
 * inputs	- pointer to client to report to
 * output	- none
 * side effects	- admin info is sent to client given
 */
static void
do_admin(struct Client *source_p)
{
	sendto_one_numeric(source_p, RPL_ADMINME, form_str(RPL_ADMINME), me.name);
	if(AdminInfo.name != NULL)
		sendto_one_numeric(source_p, RPL_ADMINLOC1, form_str(RPL_ADMINLOC1), AdminInfo.name);
	if(AdminInfo.description != NULL)
		sendto_one_numeric(source_p, RPL_ADMINLOC2, form_str(RPL_ADMINLOC2), AdminInfo.description);
	if(AdminInfo.email != NULL)
		sendto_one_numeric(source_p, RPL_ADMINEMAIL, form_str(RPL_ADMINEMAIL), AdminInfo.email);
}
