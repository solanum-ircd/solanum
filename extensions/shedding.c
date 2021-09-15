/*
 *  Solanum: a slightly advanced ircd
 *  shedding.c: Enables/disables user shedding.
 *
 *  Based on oftc-hybrid's m_shedding.c
 *
 *  Copyright (C) 2021 David Schultz <me@zpld.me>
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
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
 *
 */

#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_serv.h"
#include "s_newconf.h"
#include "messages.h"
#include "numeric.h"


#define SHED_RATE_MIN 5

static int rate = 60;

static struct ev_entry *user_shedding_main_ev = NULL;
static struct ev_entry *user_shedding_shed_ev = NULL;

static const char shed_desc[] = "Enables/disables user shedding.";

static void mo_shedding(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static void user_shedding_main(void *rate);
static void user_shedding_shed(void *unused);

static struct Message shedding_msgtab = {
	"SHEDDING", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, {mo_shedding, 3}, mg_ignore, mg_ignore, {mo_shedding, 3}}
};

mapi_clist_av1 shedding_clist[] = { &shedding_msgtab, NULL };

static void
moddeinit(void)
{
	rb_event_delete(user_shedding_main_ev);
	rb_event_delete(user_shedding_shed_ev);
}

DECLARE_MODULE_AV2(shed, NULL, moddeinit, shedding_clist, NULL, NULL, NULL, NULL, shed_desc);

/*
 * mo_shedding
 *
 * inputs - pointer to server
 *    - pointer to client
 *    - parameter count
 *    - parameter list
 * output -
 * side effects - user shedding is enabled or disabled
 *
 * SHEDDING <server> OFF - disable shedding
 * SHEDDING <server> <approx_seconds_per_userdrop> :<reason>
 * (parv[#] 1        2                             3)
 *
 */
static void
mo_shedding(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if (!HasPrivilege(source_p, "oper:shedding"))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "SHEDDING");
		return;
	}

	if (parc != 4 && !(parc == 3 && irccmp(parv[2], "OFF") == 0))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			me.name, source_p->name, "SHEDDING");
		return;
	}

	if (hunt_server(client_p, source_p, ":%s SHEDDING %s %s %s :%s", 1,
		parc, parv) != HUNTED_ISME)
		return;

	if (!irccmp(parv[2], "OFF"))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL | L_NETWIDE, "%s disabled user shedding", get_oper_name(source_p));
		rb_event_delete(user_shedding_main_ev);
		user_shedding_main_ev = NULL;
		rb_event_delete(user_shedding_shed_ev);
		user_shedding_shed_ev = NULL;
		return;
	}

	rate = atoi(parv[2]);

	if(rate < SHED_RATE_MIN)
		rate = SHED_RATE_MIN;

	sendto_realops_snomask(SNO_GENERAL, L_ALL | L_NETWIDE, "%s enabled user shedding (interval: %d seconds, reason: %s)",
		get_oper_name(source_p), rate, parv[3]);

	rate -= (rate/5);
	rb_event_delete(user_shedding_main_ev);
	user_shedding_main_ev = NULL;
	user_shedding_main_ev = rb_event_add("user shedding main event", user_shedding_main, NULL, rate);
}

static void
user_shedding_main(void *unused)
{
	int deviation = (rate / (3+(int) (7.0f*rand()/(RAND_MAX+1.0f))));

	user_shedding_shed_ev = rb_event_addish("user shedding shed event", user_shedding_shed, NULL, rate+deviation);
}

static void
user_shedding_shed(void *unused)
{
	rb_dlink_node *ptr;
	struct Client *client_p;

	RB_DLINK_FOREACH_PREV(ptr, lclient_list.tail)
	{
		client_p = ptr->data;

		if (!MyClient(client_p)) /* It could be servers */
			continue;
		if (IsExemptKline(client_p))
			continue;
		exit_client(client_p, client_p, &me, "Connection closed");
		break;
	}

	rb_event_delete(user_shedding_shed_ev);
	user_shedding_shed_ev = NULL;
}
