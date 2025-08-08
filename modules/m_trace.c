/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_trace.c: Traces a path to a client/server.
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
#include "class.h"
#include "hook.h"
#include "client.h"
#include "hash.h"
#include "hash.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static const char trace_desc[] =
	"Provides the TRACE command to trace the route to a client or server";

static void m_trace(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message trace_msgtab = {
	"TRACE", 0, 0, 0, 0,
	{mg_unreg, {m_trace, 0}, {m_trace, 0}, mg_ignore, mg_ignore, {m_trace, 0}}
};

int doing_trace_hook;
int doing_trace_show_idle_hook;

mapi_clist_av1 trace_clist[] = { &trace_msgtab, NULL };
mapi_hlist_av1 trace_hlist[] = {
	{ "doing_trace",	&doing_trace_hook },
	{ "doing_trace_show_idle", &doing_trace_show_idle_hook },
	{ NULL, NULL }
};
DECLARE_MODULE_AV2(trace, NULL, NULL, trace_clist, trace_hlist, NULL, NULL, NULL, trace_desc);

static void count_downlinks(struct Client *server_p, int *pservcount, int *pusercount);
static int report_this_status(struct Client *source_p, struct Client *target_p);

static const char *empty_sockhost = "255.255.255.255";

/*
 * m_trace
 *      parv[1] = servername
 */
static void
m_trace(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p = NULL;
	struct Class *cltmp;
	const char *tname;
	bool doall = false, wilds, dow;
	int cnt = 0;
	rb_dlink_node *ptr;

	if(parc > 1)
	{
		tname = parv[1];

		if(parc > 2)
		{
			if(hunt_server(client_p, source_p, ":%s TRACE %s :%s", 2, parc, parv) !=
					HUNTED_ISME)
				return;
		}
	}
	else
		tname = me.name;

	/* if we have 3 parameters, then the command is directed at us.  So
	 * we shouldnt be forwarding it anywhere.
	 */
	if(parc < 3)
	{
		switch (hunt_server(client_p, source_p, ":%s TRACE :%s", 1, parc, parv))
		{
		case HUNTED_PASS:	/* note: gets here only if parv[1] exists */
		{
			struct Client *ac2ptr;

			if(MyClient(source_p))
				ac2ptr = find_named_client(tname);
			else
				ac2ptr = find_client(tname);

			if(ac2ptr == NULL)
			{
				RB_DLINK_FOREACH(ptr, global_serv_list.head)
				{
					ac2ptr = ptr->data;

					if(match(tname, ac2ptr->name))
						break;
					else
						ac2ptr = NULL;
				}
			}

			/* giving this out with flattened links defeats the
			 * object --fl
			 */
			if(IsOperGeneral(source_p) || IsExemptShide(source_p) ||
			   !ConfigServerHide.flatten_links)
				sendto_one_numeric(source_p, RPL_TRACELINK,
						   form_str(RPL_TRACELINK),
						   ircd_version,
						   ac2ptr ? ac2ptr->name : tname,
						   ac2ptr ? ac2ptr->from->name : "EEK!");

			return;
		}

		case HUNTED_ISME:
			break;

		default:
			return;
		}
	}

	if(match(tname, me.name))
	{
		doall = true;
	}
	/* if theyre tracing our SID, we need to move tname to our name so
	 * we dont give the sid in ENDOFTRACE
	 */
	else if(!MyClient(source_p) && !strcmp(tname, me.id))
	{
		doall = true;
		tname = me.name;
	}

	wilds = strchr(tname, '*') || strchr(tname, '?');
	dow = wilds || doall;

	/* specific trace */
	if(!dow)
	{
		if(MyClient(source_p) || parc > 2)
			target_p = find_named_person(tname);
		else
			target_p = find_person(tname);

		/* tname could be pointing to an ID at this point, so reset
		 * it to target_p->name if we have a target --fl
		 */
		if(target_p != NULL)
		{
			report_this_status(source_p, target_p);
			tname = target_p->name;
		}

		sendto_one_numeric(source_p, RPL_ENDOFTRACE,
				   form_str(RPL_ENDOFTRACE), tname);
		return;
	}

	/* give non-opers a limited trace output of themselves (if local),
	 * opers and servers (if no shide) --fl
	 */
	if(!IsOper(source_p))
	{
		if(MyClient(source_p))
		{
			if(doall || (wilds && match(tname, source_p->name)))
				report_this_status(source_p, source_p);
		}

		RB_DLINK_FOREACH(ptr, local_oper_list.head)
		{
			target_p = ptr->data;

			if(!doall && wilds && (match(tname, target_p->name) == 0))
				continue;

			if(!SeesOper(target_p, source_p))
				continue;

			report_this_status(source_p, target_p);
		}

		if (IsExemptShide(source_p) || !ConfigServerHide.flatten_links)
		{
			RB_DLINK_FOREACH(ptr, serv_list.head)
			{
				target_p = ptr->data;

				if(!doall && wilds && !match(tname, target_p->name))
					continue;

				report_this_status(source_p, target_p);
			}
		}

		sendto_one_numeric(source_p, RPL_ENDOFTRACE,
				   form_str(RPL_ENDOFTRACE), tname);
		return;
	}

	/* source_p is opered */

	/* report all direct connections */
	RB_DLINK_FOREACH(ptr, lclient_list.head)
	{
		target_p = ptr->data;

		/* dont show invisible users to remote opers */
		if(IsInvisible(target_p) && dow && !MyConnect(source_p) && !SeesOper(target_p, source_p))
			continue;

		if(!doall && wilds && !match(tname, target_p->name))
			continue;

		/* remote opers may not see invisible normal users */
		if(dow && !MyConnect(source_p) && !SeesOper(target_p, source_p) &&
				IsInvisible(target_p))
			continue;

		cnt = report_this_status(source_p, target_p);
	}

	RB_DLINK_FOREACH(ptr, serv_list.head)
	{
		target_p = ptr->data;

		if(!doall && wilds && !match(tname, target_p->name))
			continue;

		cnt = report_this_status(source_p, target_p);
	}

	if(MyConnect(source_p))
	{
		RB_DLINK_FOREACH(ptr, unknown_list.head)
		{
			target_p = ptr->data;

			if(!doall && wilds && !match(tname, target_p->name))
				continue;

			cnt = report_this_status(source_p, target_p);
		}
	}

	if(!cnt)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHSERVER, form_str(ERR_NOSUCHSERVER),
					tname);

		/* let the user have some idea that its at the end of the
		 * trace
		 */
		sendto_one_numeric(source_p, RPL_ENDOFTRACE,
				   form_str(RPL_ENDOFTRACE), tname);
		return;
	}

	if(doall)
	{
		RB_DLINK_FOREACH(ptr, class_list.head)
		{
			cltmp = ptr->data;

			if(CurrUsers(cltmp) > 0)
				sendto_one_numeric(source_p, RPL_TRACECLASS,
						   form_str(RPL_TRACECLASS),
						   ClassName(cltmp), CurrUsers(cltmp));
		}
	}

	sendto_one_numeric(source_p, RPL_ENDOFTRACE, form_str(RPL_ENDOFTRACE), tname);
}

/*
 * count_downlinks
 *
 * inputs	- pointer to server to count
 *		- pointers to server and user count
 * output	- NONE
 * side effects - server and user counts are added to given values
 */
static void
count_downlinks(struct Client *server_p, int *pservcount, int *pusercount)
{
	rb_dlink_node *ptr;

	(*pservcount)++;
	*pusercount += rb_dlink_list_length(&server_p->serv->users);
	RB_DLINK_FOREACH(ptr, server_p->serv->servers.head)
	{
		count_downlinks(ptr->data, pservcount, pusercount);
	}
}

/*
 * report_this_status
 *
 * inputs	- pointer to client to report to
 * 		- pointer to client to report about
 * output	- counter of number of hits
 * side effects - NONE
 */
static int
report_this_status(struct Client *source_p, struct Client *target_p)
{
	const char *name;
	const char *class_name;
	char ip[HOSTIPLEN];
	int cnt = 0;

	/* sanity check - should never happen */
	if(!MyConnect(target_p))
		return 0;

	rb_inet_ntop_sock((struct sockaddr *)&target_p->localClient->ip, ip, sizeof(ip));
	class_name = get_client_class(target_p);

	if(IsAnyServer(target_p))
		name = target_p->name;
	else
		name = get_client_name(target_p, HIDE_IP);

	switch (target_p->status)
	{
	case STAT_CONNECTING:
		sendto_one_numeric(source_p, RPL_TRACECONNECTING,
				form_str(RPL_TRACECONNECTING),
				class_name, name);
		cnt++;
		break;

	case STAT_HANDSHAKE:
		sendto_one_numeric(source_p, RPL_TRACEHANDSHAKE,
				form_str(RPL_TRACEHANDSHAKE),
				class_name, name);
		cnt++;
		break;

	case STAT_ME:
		break;

	case STAT_UNKNOWN:
		/* added time -Taner */
		sendto_one_numeric(source_p, RPL_TRACEUNKNOWN,
				   form_str(RPL_TRACEUNKNOWN),
				   class_name, name, ip,
				   (unsigned long)(rb_current_time() - target_p->localClient->firsttime));
		cnt++;
		break;

	case STAT_CLIENT:
		{
			/* fire the doing_trace_show_idle hook to allow modules to tell us whether to show the idle time */
	                hook_data_client_approval hdata_showidle;

	                hdata_showidle.client = source_p;
	                hdata_showidle.target = target_p;
	                hdata_showidle.approved = WHOIS_IDLE_SHOW;

			call_hook(doing_trace_show_idle_hook, &hdata_showidle);

			sendto_one_numeric(source_p,
					SeesOper(target_p, source_p) ? RPL_TRACEOPERATOR : RPL_TRACEUSER,
					SeesOper(target_p, source_p) ? form_str(RPL_TRACEOPERATOR) : form_str(RPL_TRACEUSER),
					class_name, name,
					show_ip(source_p, target_p) ? ip : empty_sockhost,
					hdata_showidle.approved ? (unsigned long)(rb_current_time() - target_p->localClient->lasttime) : 0,
					hdata_showidle.approved ? (unsigned long)(rb_current_time() - target_p->localClient->last) : 0);

			cnt++;
		}
		break;

	case STAT_SERVER:
		{
			int usercount = 0;
			int servcount = 0;

			count_downlinks(target_p, &servcount, &usercount);

			sendto_one_numeric(source_p, RPL_TRACESERVER, form_str(RPL_TRACESERVER),
				   class_name, servcount, usercount, name,
				   *(target_p->serv->by) ? target_p->serv->by : "*", "*",
				   me.name,
				   (unsigned long)(rb_current_time() - target_p->localClient->lasttime));
			cnt++;

		}
		break;

	default:		/* ...we actually shouldn't come here... --msa */
		sendto_one_numeric(source_p, RPL_TRACENEWTYPE,
				   form_str(RPL_TRACENEWTYPE), name);
		cnt++;
		break;
	}

	return (cnt);
}
