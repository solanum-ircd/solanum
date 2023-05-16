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

static struct ev_entry *user_shedding_ev = NULL;

static const char shed_desc[] = "Enables/disables user shedding.";

static void mo_shedding(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static void me_shedding(struct MsgBuf *msgbuf, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static void do_user_shedding(void *unused);

static struct Message shedding_msgtab = {
	"SHEDDING", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_shedding, 2}, {mo_shedding, 3}}
};

mapi_clist_av1 shedding_clist[] = { &shedding_msgtab, NULL };

static void
moddeinit(void)
{
	rb_event_delete(user_shedding_ev);
}

DECLARE_MODULE_AV2(shed, NULL, moddeinit, shedding_clist, NULL, NULL, NULL, NULL, shed_desc);

static void
set_shedding_state(struct Client *source_p, const char *chr, const char *reason)
{
	if (strcmp(chr, "OFF") == 0)
	{
		// disable shedding
		sendto_realops_snomask(SNO_GENERAL, L_ALL | L_NETWIDE, "%s disabled user shedding", get_oper_name(source_p));
		rb_event_delete(user_shedding_ev);
		user_shedding_ev = NULL;
		return;
	}

	rate = atoi(chr);

	if(rate < SHED_RATE_MIN)
	{
		sendto_one_notice(source_p, "Shedding rate must be at least %d", SHED_RATE_MIN);
		return;
	}

	sendto_realops_snomask(SNO_GENERAL, L_ALL | L_NETWIDE, "%s enabled user shedding (interval: %d seconds, reason: %s)",
	get_oper_name(source_p), rate, reason);

	rb_event_delete(user_shedding_ev);
	user_shedding_ev = NULL;
	user_shedding_ev = rb_event_add("user shedding event", do_user_shedding, NULL, rate);
}

static bool
contains_wildcard(const char *p)
{
    while(*p) {
        if (*p == '*')
            return true;
        p++;
    }
    return false;
}

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

	/* I can think of a thousand ways this could go wrong... */
	if (contains_wildcard(parv[1]))
	{
		sendto_one_notice(source_p, "Wildcards are not permitted for shedding targets");
		return;
	}

	if (parc != 4 && !(parc == 3 && irccmp(parv[2], "OFF") == 0))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
			me.name, source_p->name, "SHEDDING");
		return;
	}

	if (irccmp(parv[1], me.name) != 0) {
		/* it's not for us, pass it around */
		if (irccmp(parv[2], "OFF") == 0)
			sendto_match_servs(source_p, parv[1],
				CAP_ENCAP, NOCAPS,
				"ENCAP %s SHEDDING OFF", parv[1]);
		else
			sendto_match_servs(source_p, parv[1],
				CAP_ENCAP, NOCAPS,
				"ENCAP %s SHEDDING %s :%s",
				parv[1], parv[2], parv[3]);
		return;
	}

	set_shedding_state(source_p, parv[2], parv[3]);
}

static void
me_shedding(struct MsgBuf *msgbuf, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(!IsPerson(source_p))
		return;

	set_shedding_state(source_p, parv[1], parv[2]);

	return;
}


static void
do_user_shedding(void *unused)
{
	rb_dlink_node *ptr;
	struct Client *client_p;

	RB_DLINK_FOREACH_PREV(ptr, lclient_list.tail)
	{
		client_p = ptr->data;

		if (!IsClient(client_p)) /* It could be servers */
			continue;
		if (IsExemptKline(client_p))
			continue;
		exit_client(client_p, client_p, &me, "Server closed connection");
		break;
	}
}
