/*
 *  ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *  m_stats.c: Sends the user statistics or config information.
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
#include "class.h"		/* report_classes */
#include "client.h"		/* Client */
#include "match.h"
#include "ircd.h"		/* me */
#include "listener.h"		/* show_ports */
#include "msg.h"		/* Message */
#include "hostmask.h"		/* report_mtrie_conf_links */
#include "numeric.h"		/* ERR_xxx */
#include "scache.h"		/* list_scache */
#include "send.h"		/* sendto_one */
#include "s_conf.h"		/* ConfItem */
#include "s_serv.h"		/* hunt_server */
#include "s_stats.h"
#include "s_user.h"		/* show_opers */
#include "parse.h"
#include "modules.h"
#include "hook.h"
#include "s_newconf.h"
#include "hash.h"
#include "reject.h"
#include "whowas.h"
#include "rb_radixtree.h"
#include "sslproc.h"
#include "s_assert.h"

static const char stats_desc[] =
	"Provides the STATS command to inspect various server/network information";

static void m_stats (struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message stats_msgtab = {
	"STATS", false, false, false, false,
	{mg_unreg, {m_stats, 2}, {m_stats, 3}, mg_ignore, mg_ignore, {m_stats, 2}}
};

int doing_stats_hook;
int doing_stats_p_hook;
int doing_stats_show_idle_hook;

mapi_clist_av1 stats_clist[] = { &stats_msgtab, NULL };
mapi_hlist_av1 stats_hlist[] = {
	{ "doing_stats",	&doing_stats_hook },
	{ "doing_stats_p",	&doing_stats_p_hook },
	{ "doing_stats_show_idle", &doing_stats_show_idle_hook },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(stats, NULL, NULL, stats_clist, stats_hlist, NULL, NULL, NULL, stats_desc);

const char *Lformat = "%s %d %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32" :%"PRId64" %"PRId64" %s";

static void stats_l_list(struct Client *s, const char *, bool, bool, rb_dlink_list *, char,
				bool (*check_fn)(struct Client *source_p, struct Client *target_p));
static void stats_l_client(struct Client *source_p, struct Client *target_p,
				char statchar);

typedef void (*handler_t)(struct Client *source_p);
typedef void (*handler_parv_t)(struct Client *source_p, int parc, const char *parv[]);

struct stats_cmd
{
	union
	{
		handler_t handler;
		handler_parv_t handler_parv;
	};
	const char *need_priv;
	bool need_parv;
	bool need_admin;
};

static void stats_dns_servers(struct Client *);
static void stats_delay(struct Client *);
static void stats_hash(struct Client *);
static void stats_connect(struct Client *);
static void stats_tdeny(struct Client *);
static void stats_deny(struct Client *);
static void stats_exempt(struct Client *);
static void stats_events(struct Client *);
static void stats_prop_klines(struct Client *);
static void stats_auth(struct Client *);
static void stats_tklines(struct Client *);
static void stats_klines(struct Client *);
static void stats_messages(struct Client *);
static void stats_dnsbl(struct Client *);
static void stats_oper(struct Client *);
static void stats_privset(struct Client *);
static void stats_operedup(struct Client *);
static void stats_ports(struct Client *);
static void stats_tresv(struct Client *);
static void stats_resv(struct Client *);
static void stats_secure(struct Client *);
static void stats_ssld(struct Client *);
static void stats_usage(struct Client *);
static void stats_tstats(struct Client *);
static void stats_uptime(struct Client *);
static void stats_servers(struct Client *);
static void stats_tgecos(struct Client *);
static void stats_gecos(struct Client *);
static void stats_class(struct Client *);
static void stats_memory(struct Client *);
static void stats_servlinks(struct Client *);
static void stats_ltrace(struct Client *, int, const char **);
static void stats_comm(struct Client *);
static void stats_capability(struct Client *);

#define HANDLER_NORM(fn, admin, priv) \
		{ { .handler = fn }, .need_parv = false, .need_priv = priv, .need_admin = admin }
#define HANDLER_PARV(fn, admin, priv) \
		{ { .handler_parv = fn }, .need_parv = true, .need_priv = priv, .need_admin = admin }

/* This table contains the possible stats items, in order:
 * stats letter,  function to call, operonly? adminonly? --fl_
 *
 * Previously in this table letters were a column. I fixed it to use modern
 * C initalisers so we don't have to iterate anymore
 * --Elizafox
 */
static struct stats_cmd stats_cmd_table[256] = {
/*	letter               handler		admin	priv */
	['a'] = HANDLER_NORM(stats_dns_servers,	true,	NULL),
	['A'] = HANDLER_NORM(stats_dns_servers,	true,	NULL),
	['b'] = HANDLER_NORM(stats_delay,	true,	NULL),
	['B'] = HANDLER_NORM(stats_hash,	true,	NULL),
	['c'] = HANDLER_NORM(stats_connect,	false,	NULL),
	['C'] = HANDLER_NORM(stats_capability,	false,	"oper:general"),
	['d'] = HANDLER_NORM(stats_tdeny,	false,	"oper:general"),
	['D'] = HANDLER_NORM(stats_deny,	false,	"oper:general"),
	['e'] = HANDLER_NORM(stats_exempt,	false,	"oper:general"),
	['E'] = HANDLER_NORM(stats_events,	true,	NULL),
	['f'] = HANDLER_NORM(stats_comm,	true,	NULL),
	['F'] = HANDLER_NORM(stats_comm,	true,	NULL),
	['g'] = HANDLER_NORM(stats_prop_klines,	false,	"oper:general"),
	['i'] = HANDLER_NORM(stats_auth,	false,	NULL),
	['I'] = HANDLER_NORM(stats_auth,	false,	NULL),
	['k'] = HANDLER_NORM(stats_tklines,	false,	NULL),
	['K'] = HANDLER_NORM(stats_klines,	false,	NULL),
	['l'] = HANDLER_PARV(stats_ltrace,	false,	NULL),
	['L'] = HANDLER_PARV(stats_ltrace,	false,	NULL),
	['m'] = HANDLER_NORM(stats_messages,	false,	NULL),
	['M'] = HANDLER_NORM(stats_messages,	false,	NULL),
	['n'] = HANDLER_NORM(stats_dnsbl,	false,	NULL),
	['o'] = HANDLER_NORM(stats_oper,	false,	NULL),
	['O'] = HANDLER_NORM(stats_privset,	false,	"oper:privs"),
	['p'] = HANDLER_NORM(stats_operedup,	false,	NULL),
	['P'] = HANDLER_NORM(stats_ports,	false,	NULL),
	['q'] = HANDLER_NORM(stats_tresv,	false,	"oper:general"),
	['Q'] = HANDLER_NORM(stats_resv,	false,	"oper:general"),
	['r'] = HANDLER_NORM(stats_usage,	false,	"oper:general"),
	['R'] = HANDLER_NORM(stats_usage,	false,	"oper:general"),
	['s'] = HANDLER_NORM(stats_secure,	false,	"oper:general"),
	['S'] = HANDLER_NORM(stats_ssld,	true,	NULL),
	['t'] = HANDLER_NORM(stats_tstats,	false,	"oper:general"),
	['T'] = HANDLER_NORM(stats_tstats,	false,	"oper:general"),
	['u'] = HANDLER_NORM(stats_uptime,	false,	NULL),
	['v'] = HANDLER_NORM(stats_servers,	false,	NULL),
	['V'] = HANDLER_NORM(stats_servers,	false,	NULL),
	['x'] = HANDLER_NORM(stats_tgecos,	false,	"oper:general"),
	['X'] = HANDLER_NORM(stats_gecos,	false,	"oper:general"),
	['y'] = HANDLER_NORM(stats_class,	false,	NULL),
	['Y'] = HANDLER_NORM(stats_class,	false,	NULL),
	['z'] = HANDLER_NORM(stats_memory,	false,	"oper:general"),
	['?'] = HANDLER_NORM(stats_servlinks,	false,	NULL),
};

/*
 * m_stats by fl_
 * Modified heavily by Elizafox
 *      parv[1] = stat letter/command
 *      parv[2] = (if present) server/mask in stats L, or target
 *
 * This will search the tables for the appropriate stats letter,
 * if found execute it.
 */
static void
m_stats(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0;
	struct stats_cmd *cmd;
	unsigned char statchar;

	statchar = parv[1][0];

	if(MyClient(source_p) && !IsOperGeneral(source_p) && parc > 2)
	{
		/* Check the user is actually allowed to do /stats, and isnt flooding */
		if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
		{
			/* safe enough to give this on a local connect only */
			sendto_one(source_p, form_str(RPL_LOAD2HI),
				   me.name, source_p->name, "STATS");
			sendto_one_numeric(source_p, RPL_ENDOFSTATS,
					   form_str(RPL_ENDOFSTATS), statchar);
			return;
		}
		else
			last_used = rb_current_time();
	}

	if(hunt_server(client_p, source_p, ":%s STATS %s :%s", 2, parc, parv) != HUNTED_ISME)
		return;

	/* Look up */
	cmd = &stats_cmd_table[statchar];
	if(cmd->handler != NULL)
	{
		/* The stats table says what privs are needed, so check --fl_ */
		const char *missing_priv = NULL;
		if(cmd->need_admin && !IsOperAdmin(source_p))
			missing_priv = "admin";
		else if(cmd->need_priv && !HasPrivilege(source_p, cmd->need_priv))
			missing_priv = cmd->need_priv;

		if(missing_priv != NULL)
		{
			if(!IsOper(source_p))
			{
				sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
					form_str(ERR_NOPRIVILEGES));
			}
			else
			{
				if(!strncmp(missing_priv, "oper:", 5))
					missing_priv += 5;
				sendto_one(source_p, form_str(ERR_NOPRIVS),
					me.name, source_p->name, missing_priv);
			}
			goto stats_out;
		}

		if(cmd->need_parv)
			cmd->handler_parv(source_p, parc, parv);
		else
			cmd->handler(source_p);
	}

stats_out:
	/* Send the end of stats notice */
	sendto_one_numeric(source_p, RPL_ENDOFSTATS,
			   form_str(RPL_ENDOFSTATS), statchar);
}

static void
stats_dns_servers (struct Client *source_p)
{
	rb_dlink_node *n;

	RB_DLINK_FOREACH(n, nameservers.head)
	{
		sendto_one_numeric(source_p, RPL_STATSDEBUG, "A :%s", (char *)n->data);
	}
}

static void
stats_delay(struct Client *source_p)
{
	struct nd_entry *nd;
	rb_dictionary_iter iter;

	RB_DICTIONARY_FOREACH(nd, &iter, nd_dict)
	{
		sendto_one_notice(source_p, ":Delaying: %s for %ld",
				nd->name, (long) nd->expire);
	}
}

static void
stats_hash_cb(const char *buf, void *client_p)
{
	sendto_one_numeric(client_p, RPL_STATSDEBUG, "B :%s", buf);
}

static void
stats_hash(struct Client *source_p)
{
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "B :%-30s %-15s %-10s %-10s %-10s %-10s",
		"NAME", "TYPE", "OBJECTS", "DEPTH SUM", "AVG DEPTH", "MAX DEPTH");

	rb_dictionary_stats_walk(stats_hash_cb, source_p);
	rb_radixtree_stats_walk(stats_hash_cb, source_p);
}

static void
stats_connect(struct Client *source_p)
{
	static char buf[BUFSIZE];
	struct server_conf *server_p;
	char *s;
	rb_dlink_node *ptr;

	if((ConfigFileEntry.stats_c_oper_only ||
	    (ConfigServerHide.flatten_links && !IsExemptShide(source_p))) &&
	    !IsOperGeneral(source_p))
	{
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str(ERR_NOPRIVILEGES));
		return;
	}

	RB_DLINK_FOREACH(ptr, server_conf_list.head)
	{
		server_p = ptr->data;

		if(ServerConfIllegal(server_p))
			continue;

		s = buf;

		if(IsOperGeneral(source_p))
		{
			if(ServerConfAutoconn(server_p))
				*s++ = 'A';
			if(ServerConfSCTP(server_p))
				*s++ = 'M';
			if(ServerConfSSL(server_p))
				*s++ = 'S';
			if(ServerConfTb(server_p))
				*s++ = 'T';
		}

		if(s == buf)
			*s++ = '*';

		*s = '\0';

		sendto_one_numeric(source_p, RPL_STATSCLINE,
				form_str(RPL_STATSCLINE),
				"*@127.0.0.1",
				buf, server_p->name,
				server_p->port, server_p->class_name,
				server_p->certfp ? server_p->certfp : "*");
	}
}

/* stats_tdeny()
 *
 * input	- client to report to
 * output	- none
 * side effects - client is given temp dline list.
 */
static void
stats_tdeny (struct Client *source_p)
{
	char *host, *pass, *user, *oper_reason;
	struct AddressRec *arec;
	struct ConfItem *aconf;
	int i;

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_DLINE)
			{
				aconf = arec->aconf;

				if(!(aconf->flags & CONF_FLAGS_TEMPORARY))
					continue;

				get_printable_kline(source_p, aconf, &host, &pass, &user, &oper_reason);

				sendto_one_numeric(source_p, RPL_STATSDLINE,
						   form_str (RPL_STATSDLINE),
						   'd', host, pass,
						   oper_reason ? "|" : "",
						   oper_reason ? oper_reason : "");
			}
		}
	}
}

/* stats_deny()
 *
 * input	- client to report to
 * output	- none
 * side effects - client is given dline list.
 */
static void
stats_deny (struct Client *source_p)
{
	char *host, *pass, *user, *oper_reason;
	struct AddressRec *arec;
	struct ConfItem *aconf;
	int i;

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_DLINE)
			{
				aconf = arec->aconf;

				if(aconf->flags & CONF_FLAGS_TEMPORARY)
					continue;

				get_printable_kline(source_p, aconf, &host, &pass, &user, &oper_reason);

				sendto_one_numeric(source_p, RPL_STATSDLINE,
						   form_str (RPL_STATSDLINE),
						   'D', host, pass,
						   oper_reason ? "|" : "",
						   oper_reason ? oper_reason : "");
			}
		}
	}
}


/* stats_exempt()
 *
 * input	- client to report to
 * output	- none
 * side effects - client is given list of exempt blocks
 */
static void
stats_exempt(struct Client *source_p)
{
	char *name, *host, *user, *classname, *desc;
	const char *pass;
	struct AddressRec *arec;
	struct ConfItem *aconf;
	int i, port;

	if(ConfigFileEntry.stats_e_disabled)
	{
		sendto_one_numeric(source_p, ERR_DISABLED,
				   form_str(ERR_DISABLED), "STATS e");
		return;
	}

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_EXEMPTDLINE)
			{
				aconf = arec->aconf;
				get_printable_conf (aconf, &name, &host, &pass,
						    &user, &port, &classname, &desc);

				sendto_one_numeric(source_p, RPL_STATSDLINE,
						   form_str(RPL_STATSDLINE),
						   'e', host, pass, "", "");
			}
		}
	}
}


static void
stats_events_cb(char *str, void *ptr)
{
	sendto_one_numeric(ptr, RPL_STATSDEBUG, "E :%s", str);
}

static void
stats_events (struct Client *source_p)
{
	rb_dump_events(stats_events_cb, source_p);
}

static void
stats_prop_klines(struct Client *source_p)
{
	struct ConfItem *aconf;
	char *user, *host, *pass, *oper_reason;
	rb_dictionary_iter state;

	RB_DICTIONARY_FOREACH(aconf, &state, prop_bans_dict)
	{
		/* Skip non-klines and deactivated klines. */
		if (aconf->status != CONF_KILL)
			continue;

		get_printable_kline(source_p, aconf, &host, &pass,
				&user, &oper_reason);

		sendto_one_numeric(source_p, RPL_STATSKLINE,
				form_str(RPL_STATSKLINE),
				'g', host, user, pass,
				oper_reason ? "|" : "",
				oper_reason ? oper_reason : "");
	}
}

static void
stats_auth (struct Client *source_p)
{
	/* Oper only, if unopered, return ERR_NOPRIVS */
	if((ConfigFileEntry.stats_i_oper_only == 2) && !IsOperGeneral (source_p))
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));

	/* If unopered, Only return matching auth blocks */
	else if((ConfigFileEntry.stats_i_oper_only == 1) && !IsOperGeneral (source_p))
	{
		struct ConfItem *aconf;
		char *name, *host, *user, *classname, *desc;
		const char *pass = "*";
		int port;

		if(MyConnect (source_p))
			aconf = find_conf_by_address (source_p->host, source_p->sockhost, NULL,
						      (struct sockaddr *)&source_p->localClient->ip,
						      CONF_CLIENT,
						      GET_SS_FAMILY(&source_p->localClient->ip),
						      source_p->username, NULL);
		else
			aconf = find_conf_by_address (source_p->host, NULL, NULL, NULL, CONF_CLIENT,
						      0, source_p->username, NULL);

		if(aconf == NULL)
			return;

		get_printable_conf (aconf, &name, &host, &pass, &user, &port, &classname, &desc);
		if(!EmptyString(aconf->spasswd))
			pass = aconf->spasswd;

		sendto_one_numeric(source_p, RPL_STATSILINE, form_str(RPL_STATSILINE),
				   name, pass, show_iline_prefix(source_p, aconf, user),
				   host, port, classname, desc);
	}

	/* Theyre opered, or allowed to see all auth blocks */
	else
		report_auth (source_p);
}


static void
stats_tklines(struct Client *source_p)
{
	/* Oper only, if unopered, return ERR_NOPRIVS */
	if((ConfigFileEntry.stats_k_oper_only == 2) && !IsOperGeneral (source_p))
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));

	/* If unopered, Only return matching klines */
	else if((ConfigFileEntry.stats_k_oper_only == 1) && !IsOperGeneral (source_p))
	{
		struct ConfItem *aconf;
		char *host, *pass, *user, *oper_reason;

		if(MyConnect (source_p))
			aconf = find_conf_by_address (source_p->host, source_p->sockhost, NULL,
						      (struct sockaddr *)&source_p->localClient->ip,
						      CONF_KILL,
						      GET_SS_FAMILY(&source_p->localClient->ip),
						      source_p->username, NULL);
		else
			aconf = find_conf_by_address (source_p->host, NULL, NULL, NULL, CONF_KILL,
						      0, source_p->username, NULL);

		if(aconf == NULL)
			return;

		/* dont report a permanent kline as a tkline */
		if((aconf->flags & CONF_FLAGS_TEMPORARY) == 0)
			return;

		get_printable_kline(source_p, aconf, &host, &pass, &user, &oper_reason);

		sendto_one_numeric(source_p, RPL_STATSKLINE,
				   form_str(RPL_STATSKLINE), (aconf->flags & CONF_FLAGS_TEMPORARY) ? 'k' : 'K',
				   host, user, pass, oper_reason ? "|" : "",
				   oper_reason ? oper_reason : "");
	}
	/* Theyre opered, or allowed to see all klines */
	else
	{
		struct ConfItem *aconf;
		rb_dlink_node *ptr;
		int i;
		char *user, *host, *pass, *oper_reason;

		for(i = 0; i < LAST_TEMP_TYPE; i++)
		{
			RB_DLINK_FOREACH(ptr, temp_klines[i].head)
			{
				aconf = ptr->data;

				get_printable_kline(source_p, aconf, &host, &pass,
							&user, &oper_reason);

				sendto_one_numeric(source_p, RPL_STATSKLINE,
						   form_str(RPL_STATSKLINE),
						   'k', host, user, pass,
						   oper_reason ? "|" : "",
						   oper_reason ? oper_reason : "");
			}
		}
	}
}

/* report_Klines()
 *
 * inputs       - Client to report to, mask
 * outputs      -
 * side effects - Reports configured K-lines to client_p.
 */
static void
report_Klines(struct Client *source_p)
{
	char *host, *pass, *user, *oper_reason;
	struct AddressRec *arec;
	struct ConfItem *aconf = NULL;
	int i;

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_KILL)
			{
				aconf = arec->aconf;

				/* its a tempkline, theyre reported elsewhere */
				if(aconf->flags & CONF_FLAGS_TEMPORARY)
					continue;

				get_printable_kline(source_p, aconf, &host, &pass, &user, &oper_reason);
				sendto_one_numeric(source_p, RPL_STATSKLINE,
						   form_str(RPL_STATSKLINE),
						   'K', host, user, pass,
						   oper_reason ? "|" : "",
						   oper_reason ? oper_reason : "");
			}
		}
	}
}

static void
stats_klines(struct Client *source_p)
{
	/* Oper only, if unopered, return ERR_NOPRIVS */
	if((ConfigFileEntry.stats_k_oper_only == 2) && !IsOperGeneral (source_p))
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));

	/* If unopered, Only return matching klines */
	else if((ConfigFileEntry.stats_k_oper_only == 1) && !IsOperGeneral (source_p))
	{
		struct ConfItem *aconf;
		char *host, *pass, *user, *oper_reason;

		/* search for a kline */
		if(MyConnect (source_p))
			aconf = find_conf_by_address (source_p->host, source_p->sockhost, NULL,
						      (struct sockaddr *)&source_p->localClient->ip,
						      CONF_KILL,
						      GET_SS_FAMILY(&source_p->localClient->ip),
						      source_p->username, NULL);
		else
			aconf = find_conf_by_address (source_p->host, NULL, NULL, NULL, CONF_KILL,
						      0, source_p->username, NULL);

		if(aconf == NULL)
			return;

		get_printable_kline(source_p, aconf, &host, &pass, &user, &oper_reason);

		sendto_one_numeric(source_p, RPL_STATSKLINE, form_str(RPL_STATSKLINE),
				   (aconf->flags & CONF_FLAGS_TEMPORARY) ? 'k' : 'K',
				   host, user, pass, oper_reason ? "|" : "",
				   oper_reason ? oper_reason : "");
	}
	/* Theyre opered, or allowed to see all klines */
	else
		report_Klines (source_p);
}

static void
stats_messages(struct Client *source_p)
{
	rb_dictionary_iter iter;
	struct Message *msg;

	RB_DICTIONARY_FOREACH(msg, &iter, cmd_dict)
	{
		s_assert(msg->cmd != NULL);
		sendto_one_numeric(source_p, RPL_STATSCOMMANDS,
				   form_str(RPL_STATSCOMMANDS),
				   msg->cmd, msg->count,
				   msg->bytes, msg->rcount);
	}
}

static void
stats_dnsbl(struct Client *source_p)
{
	rb_dictionary_iter iter;
	struct DNSBLEntry *entry;

	if(dnsbl_stats == NULL)
		return;

	RB_DICTIONARY_FOREACH(entry, &iter, dnsbl_stats)
	{
		/* use RPL_STATSDEBUG for now -- jilles */
		sendto_one_numeric(source_p, RPL_STATSDEBUG, "n :%d %s",
				entry->hits, entry->host);
	}
}

static void
stats_oper(struct Client *source_p)
{
	struct oper_conf *oper_p;
	rb_dlink_node *ptr;

	if(!IsOperGeneral(source_p) && ConfigFileEntry.stats_o_oper_only)
	{
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
		return;
	}

	RB_DLINK_FOREACH(ptr, oper_conf_list.head)
	{
		oper_p = ptr->data;

		sendto_one_numeric(source_p, RPL_STATSOLINE,
				form_str(RPL_STATSOLINE),
				oper_p->username, oper_p->host, oper_p->name,
				HasPrivilege(source_p, "oper:privs") ? oper_p->privset->name : "0", "-1");
	}
}

static void
stats_capability_walk(const char *line, void *data)
{
	struct Client *client_p = data;

	sendto_one_numeric(client_p, RPL_STATSDEBUG, "C :%s", line);
}

static void
stats_capability(struct Client *client_p)
{
	capability_index_stats(stats_capability_walk, client_p);
}

static void
stats_privset(struct Client *source_p)
{
	privilegeset_report(source_p);
}

/* stats_operedup()
 *
 * input	- client pointer
 * output	- none
 * side effects - client is shown a list of active opers
 */
static void
stats_operedup (struct Client *source_p)
{
	struct Client *target_p;
	rb_dlink_node *oper_ptr;
	unsigned int count = 0;

	RB_DLINK_FOREACH (oper_ptr, oper_list.head)
	{
		target_p = oper_ptr->data;

		if(!SeesOper(target_p, source_p))
			continue;

		if(target_p->user->away)
			continue;

		count++;

		sendto_one_numeric(source_p, RPL_STATSDEBUG,
				   "p :%s (%s@%s)",
				   target_p->name, target_p->username,
				   target_p->host);
	}

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
				"p :%u staff members", count);
}

static void
stats_ports (struct Client *source_p)
{
	if(!IsOperGeneral (source_p) && ConfigFileEntry.stats_P_oper_only)
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
	else
		show_ports (source_p);
}

static void
stats_tresv(struct Client *source_p)
{
	struct ConfItem *aconf;
	rb_radixtree_iteration_state state;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, resv_conf_list.head)
	{
		aconf = ptr->data;
		if(aconf->hold)
			sendto_one_numeric(source_p, RPL_STATSQLINE,
					form_str(RPL_STATSQLINE),
					'q', aconf->port, aconf->host, aconf->passwd);
	}

	RB_RADIXTREE_FOREACH(aconf, &state, resv_tree)
	{
		if(aconf->hold)
			sendto_one_numeric(source_p, RPL_STATSQLINE,
					form_str(RPL_STATSQLINE),
					'q', aconf->port, aconf->host, aconf->passwd);
	}
}


static void
stats_resv(struct Client *source_p)
{
	struct ConfItem *aconf;
	rb_radixtree_iteration_state state;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, resv_conf_list.head)
	{
		aconf = ptr->data;
		if(!aconf->hold)
			sendto_one_numeric(source_p, RPL_STATSQLINE,
					form_str(RPL_STATSQLINE),
					'Q', aconf->port, aconf->host, aconf->passwd);
	}

	RB_RADIXTREE_FOREACH(aconf, &state, resv_tree)
	{
		if(!aconf->hold)
			sendto_one_numeric(source_p, RPL_STATSQLINE,
					form_str(RPL_STATSQLINE),
					'Q', aconf->port, aconf->host, aconf->passwd);
	}
}

static void
stats_secure(struct Client *source_p)
{
	struct AddressRec *arec;
	struct ConfItem *aconf;
	size_t i;

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if(arec->type == CONF_SECURE)
			{
				aconf = arec->aconf;
				sendto_one_numeric(source_p, RPL_STATSDEBUG, "s :%s", aconf->host);
			}
		}
	}
}

static void
stats_ssld_foreach(void *data, pid_t pid, int cli_count, enum ssld_status status, const char *version)
{
	struct Client *source_p = data;

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			"S :%ld %c %u :%s",
			(long)pid,
			status == SSLD_DEAD ? 'D' : (status == SSLD_SHUTDOWN ? 'S' : 'A'),
			cli_count,
			version);
}

static void
stats_ssld(struct Client *source_p)
{
	ssld_foreach_info(stats_ssld_foreach, source_p);
}

static void
stats_usage (struct Client *source_p)
{
	struct rusage rus;
	time_t secs;
	time_t rup;
#ifdef  hz
# define hzz hz
#else
# ifdef HZ
#  define hzz HZ
# else
	int hzz = 1;
# endif
#endif

	if(getrusage(RUSAGE_SELF, &rus) == -1)
	{
		sendto_one_notice(source_p, ":Getruseage error: %s.",
				  strerror(errno));
		return;
	}
	secs = rus.ru_utime.tv_sec + rus.ru_stime.tv_sec;
	if(0 == secs)
		secs = 1;

	rup = (rb_current_time() - startup_time) * hzz;
	if(0 == rup)
		rup = 1;

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "R :CPU Secs %d:%02d User %d:%02d System %d:%02d",
			   (int) (secs / 60), (int) (secs % 60),
			   (int) (rus.ru_utime.tv_sec / 60),
			   (int) (rus.ru_utime.tv_sec % 60),
			   (int) (rus.ru_stime.tv_sec / 60),
			   (int) (rus.ru_stime.tv_sec % 60));
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "R :RSS %ld ShMem %ld Data %ld Stack %ld",
			   rus.ru_maxrss, (long)(rus.ru_ixrss / rup),
			   (long)(rus.ru_idrss / rup), (long)(rus.ru_isrss / rup));
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "R :Swaps %d Reclaims %d Faults %d",
			   (int) rus.ru_nswap, (int) rus.ru_minflt, (int) rus.ru_majflt);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "R :Block in %d out %d",
			   (int) rus.ru_inblock, (int) rus.ru_oublock);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "R :Msg Rcv %d Send %d",
			   (int) rus.ru_msgrcv, (int) rus.ru_msgsnd);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "R :Signals %d Context Vol. %d Invol %d",
			   (int) rus.ru_nsignals, (int) rus.ru_nvcsw,
			   (int) rus.ru_nivcsw);
}

static void
stats_tstats (struct Client *source_p)
{
	struct Client *target_p;
	struct ServerStatistics sp;
	rb_dlink_node *ptr;

	memcpy(&sp, &ServerStats, sizeof(struct ServerStatistics));

	RB_DLINK_FOREACH(ptr, serv_list.head)
	{
		target_p = ptr->data;

		sp.is_sbs += target_p->localClient->sendB;
		sp.is_sbr += target_p->localClient->receiveB;
		sp.is_sti += (unsigned long long)(rb_current_time() - target_p->localClient->firsttime);
		sp.is_sv++;
	}

	RB_DLINK_FOREACH(ptr, lclient_list.head)
	{
		target_p = ptr->data;

		sp.is_cbs += target_p->localClient->sendB;
		sp.is_cbr += target_p->localClient->receiveB;
		sp.is_cti += (unsigned long long)(rb_current_time() - target_p->localClient->firsttime);
		sp.is_cl++;
	}

	RB_DLINK_FOREACH(ptr, unknown_list.head)
	{
		sp.is_ni++;
	}

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :accepts %u refused %u", sp.is_ac, sp.is_ref);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			"T :rejected %u delaying %lu",
			sp.is_rej, delay_exit_length());
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :throttled refused %u throttle list size %lu", sp.is_thr, throttle_size());
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			"T :nicks being delayed %lu",
			get_nd_count());
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :unknown commands %u prefixes %u",
			   sp.is_unco, sp.is_unpf);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :nick collisions %u saves %u unknown closes %u",
			   sp.is_kill, sp.is_save, sp.is_ni);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :wrong direction %u empty %u",
			   sp.is_wrdi, sp.is_empt);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :numerics seen %u", sp.is_num);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :tgchange blocked msgs %u restricted addrs %lu",
			   sp.is_tgch, rb_dlink_list_length(&tgchange_list));
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :ratelimit blocked commands %u", sp.is_rl);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :auth successes %u fails %u",
			   sp.is_asuc, sp.is_abad);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :sasl successes %u fails %u",
			   sp.is_ssuc, sp.is_sbad);
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "T :Client Server");
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "T :connected %u %u", sp.is_cl, sp.is_sv);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
				"T :bytes sent %lluK %lluK",
				sp.is_cbs / 1024,
				sp.is_sbs / 1024);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
				"T :bytes recv %lluK %lluK",
				sp.is_cbr / 1024,
				sp.is_sbr / 1024);
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
				"T :time connected %llu %llu",
				sp.is_cti, sp.is_sti);
}

static void
stats_uptime (struct Client *source_p)
{
	time_t now;

	now = rb_current_time() - startup_time;
	sendto_one_numeric(source_p, RPL_STATSUPTIME,
			   form_str (RPL_STATSUPTIME),
			   (int)(now / 86400), (int)((now / 3600) % 24),
			   (int)((now / 60) % 60), (int)(now % 60));
	sendto_one_numeric(source_p, RPL_STATSCONN,
			   form_str (RPL_STATSCONN),
			   MaxConnectionCount, MaxClientCount,
			   Count.totalrestartcount);
}


/* stats_servers()
 *
 * input	- client pointer
 * output	- none
 * side effects - client is shown lists of who connected servers
 */
static void
stats_servers (struct Client *source_p)
{
	struct Client *target_p;
	rb_dlink_node *ptr;
	time_t seconds;
	int days, hours, minutes;
	int j = 0;

	if(ConfigServerHide.flatten_links && !IsOperGeneral(source_p) &&
	   !IsExemptShide(source_p))
	{
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
		return;
	}

	RB_DLINK_FOREACH (ptr, serv_list.head)
	{
		target_p = ptr->data;

		j++;
		seconds = rb_current_time() - target_p->localClient->firsttime;

		days = (int) (seconds / 86400);
		seconds %= 86400;
		hours = (int) (seconds / 3600);
		seconds %= 3600;
		minutes = (int) (seconds / 60);
		seconds %= 60;

		sendto_one_numeric(source_p, RPL_STATSDEBUG,
				   "V :%s (%s!*@*) Idle: %d SendQ: %d "
				   "Connected: %d day%s, %d:%02d:%02d",
				   target_p->name,
				   (target_p->serv->by[0] ? target_p->serv->by : "Remote."),
				   (int) (rb_current_time() - target_p->localClient->lasttime),
				   (int) rb_linebuf_len (&target_p->localClient->buf_sendq),
				   days, (days == 1) ? "" : "s", hours, minutes,
				   (int) seconds);
	}

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "V :%d Server(s)", j);
}

static void
stats_tgecos(struct Client *source_p)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, xline_conf_list.head)
	{
		aconf = ptr->data;

		if(aconf->hold)
			sendto_one_numeric(source_p, RPL_STATSXLINE,
					form_str(RPL_STATSXLINE),
					'x', aconf->port, aconf->host,
					aconf->passwd);
	}
}

static void
stats_gecos(struct Client *source_p)
{
	struct ConfItem *aconf;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, xline_conf_list.head)
	{
		aconf = ptr->data;

		if(!aconf->hold)
			sendto_one_numeric(source_p, RPL_STATSXLINE,
					form_str(RPL_STATSXLINE),
					'X', aconf->port, aconf->host,
					aconf->passwd);
	}
}

static void
stats_class(struct Client *source_p)
{
	if(ConfigFileEntry.stats_y_oper_only && !IsOperGeneral(source_p))
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
	else
		report_classes(source_p);
}

static void
stats_memory (struct Client *source_p)
{
	struct Client *target_p;
	struct Channel *chptr;
	rb_dlink_node *rb_dlink;
	rb_dlink_node *ptr;
	int channel_count = 0;
	int local_client_conf_count = 0;	/* local client conf links */
	int users_counted = 0;	/* user structs */

	int channel_users = 0;
	int channel_invites = 0;
	int channel_bans = 0;
	int channel_except = 0;
	int channel_invex = 0;
	int channel_quiets = 0;

	int class_count = 0;	/* classes */
	int conf_count = 0;	/* conf lines */
	int users_invited_count = 0;	/* users invited */
	int user_channels = 0;	/* users in channels */
	int aways_counted = 0;
	size_t number_servers_cached;	/* number of servers cached by scache */

	size_t channel_memory = 0;
	size_t channel_ban_memory = 0;
	size_t channel_except_memory = 0;
	size_t channel_invex_memory = 0;
	size_t channel_quiet_memory = 0;

	size_t away_memory = 0;	/* memory used by aways */
	size_t ww = 0;		/* whowas array count */
	size_t wwm = 0;		/* whowas array memory used */
	size_t conf_memory = 0;	/* memory used by conf lines */
	size_t mem_servers_cached;	/* memory used by scache */

	size_t linebuf_count = 0;
	size_t linebuf_memory_used = 0;

	size_t total_channel_memory = 0;
	size_t totww = 0;

	size_t local_client_count = 0;
	size_t local_client_memory_used = 0;

	size_t remote_client_count = 0;
	size_t remote_client_memory_used = 0;

	size_t total_memory = 0;

	whowas_memory_usage(&ww, &wwm);

	RB_DLINK_FOREACH(ptr, global_client_list.head)
	{
		target_p = ptr->data;
		if(MyConnect(target_p))
		{
			local_client_conf_count++;
		}

		if(target_p->user)
		{
			users_counted++;
			users_invited_count += rb_dlink_list_length(&target_p->user->invited);
			user_channels += rb_dlink_list_length(&target_p->user->channel);
			if(target_p->user->away)
			{
				aways_counted++;
				away_memory += (strlen(target_p->user->away) + 1);
			}
		}
	}

	/* Count up all channels, ban lists, except lists, Invex lists */
	RB_DLINK_FOREACH(ptr, global_channel_list.head)
	{
		chptr = ptr->data;
		channel_count++;
		channel_memory += (strlen(chptr->chname) + sizeof(struct Channel));

		channel_users += rb_dlink_list_length(&chptr->members);
		channel_invites += rb_dlink_list_length(&chptr->invites);

		RB_DLINK_FOREACH(rb_dlink, chptr->banlist.head)
		{
			channel_bans++;

			channel_ban_memory += sizeof(rb_dlink_node) + sizeof(struct Ban);
		}

		RB_DLINK_FOREACH(rb_dlink, chptr->exceptlist.head)
		{
			channel_except++;

			channel_except_memory += (sizeof(rb_dlink_node) + sizeof(struct Ban));
		}

		RB_DLINK_FOREACH(rb_dlink, chptr->invexlist.head)
		{
			channel_invex++;

			channel_invex_memory += (sizeof(rb_dlink_node) + sizeof(struct Ban));
		}

		RB_DLINK_FOREACH(rb_dlink, chptr->quietlist.head)
		{
			channel_quiets++;

			channel_quiet_memory += (sizeof(rb_dlink_node) + sizeof(struct Ban));
		}
	}

	/* count up all classes */

	class_count = rb_dlink_list_length(&class_list) + 1;

	rb_count_rb_linebuf_memory(&linebuf_count, &linebuf_memory_used);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Users %u(%lu) Invites %u(%lu)",
			   users_counted,
			   (unsigned long) users_counted * sizeof(struct User),
			   users_invited_count,
			   (unsigned long) users_invited_count * sizeof(rb_dlink_node));

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :User channels %u(%lu) Aways %u(%zu)",
			   user_channels,
			   (unsigned long) user_channels * sizeof(rb_dlink_node),
			   aways_counted, away_memory);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Attached confs %u(%lu)",
			   local_client_conf_count,
			   (unsigned long) local_client_conf_count * sizeof(rb_dlink_node));

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Conflines %u(%zu)", conf_count, conf_memory);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Classes %u(%lu)",
			   class_count,
			   (unsigned long) class_count * sizeof(struct Class));

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Channels %u(%zu)",
			   channel_count, channel_memory);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Bans %u(%zu) Exceptions %u(%zu) Invex %u(%zu) Quiets %u(%zu)",
			   channel_bans, channel_ban_memory,
			   channel_except, channel_except_memory,
			   channel_invex, channel_invex_memory,
			   channel_quiets, channel_quiet_memory);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Channel members %u(%lu) invite %u(%lu)",
			   channel_users,
			   (unsigned long) channel_users * sizeof(rb_dlink_node),
			   channel_invites,
			   (unsigned long) channel_invites * sizeof(rb_dlink_node));

	total_channel_memory = channel_memory +
		channel_ban_memory +
		channel_users * sizeof(rb_dlink_node) + channel_invites * sizeof(rb_dlink_node);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Whowas array %zu(%zu)",
			   ww, wwm);

	totww = wwm;

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Hash: client %u(%lu) chan %u(%lu)",
			   U_MAX, (unsigned long)(U_MAX * sizeof(rb_dlink_list)),
			   CH_MAX, (unsigned long)(CH_MAX * sizeof(rb_dlink_list)));

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :linebuf %zu(%zu)",
			   linebuf_count, linebuf_memory_used);

	count_scache(&number_servers_cached, &mem_servers_cached);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :scache %zu(%zu)",
			   number_servers_cached, mem_servers_cached);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :hostname hash %d(%lu)",
			   HOST_MAX, (unsigned long)HOST_MAX * sizeof(rb_dlink_list));

	total_memory = totww + total_channel_memory + conf_memory +
		class_count * sizeof(struct Class);

	total_memory += mem_servers_cached;
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Total: whowas %zu channel %zu conf %zu",
			   totww, total_channel_memory,
			   conf_memory);

	count_local_client_memory(&local_client_count, &local_client_memory_used);
	total_memory += local_client_memory_used;

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Local client Memory in use: %zu(%zu)",
			   local_client_count, local_client_memory_used);


	count_remote_client_memory(&remote_client_count, &remote_client_memory_used);
	total_memory += remote_client_memory_used;

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :Remote client Memory in use: %zu(%zu)",
			   remote_client_count,
			   remote_client_memory_used);

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "z :TOTAL: %zu",
			   total_memory);
}

static void
stats_servlinks (struct Client *source_p)
{
	static char Sformat[] = ":%s %d %s %s %d %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32" :%"PRId64" %"PRId64" %s";
	long uptime, sendK, receiveK;
	struct Client *target_p;
	rb_dlink_node *ptr;
	int j = 0;
	char buf[128];

	if(ConfigServerHide.flatten_links && !IsOperGeneral (source_p) &&
	   !IsExemptShide(source_p))
	{
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str (ERR_NOPRIVILEGES));
		return;
	}

	sendK = receiveK = 0;

	RB_DLINK_FOREACH (ptr, serv_list.head)
	{
		target_p = ptr->data;

		j++;
		sendK += target_p->localClient->sendK;
		receiveK += target_p->localClient->receiveK;

		sendto_one(source_p, Sformat,
			get_id(&me, source_p), RPL_STATSLINKINFO, get_id(source_p, source_p),
			target_p->name,
			rb_linebuf_len(&target_p->localClient->buf_sendq),
			target_p->localClient->sendM,
			target_p->localClient->sendK,
			target_p->localClient->receiveM,
			target_p->localClient->receiveK,
			(int64_t)(rb_current_time() - target_p->localClient->firsttime),
			(int64_t)((rb_current_time() > target_p->localClient->lasttime) ?
			 (rb_current_time() - target_p->localClient->lasttime) : 0),
			IsOperGeneral (source_p) ? show_capabilities (target_p) : "TS");
	}

	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "? :%u total server(s)", j);

	snprintf(buf, sizeof buf, "%7.2f", _GMKv ((sendK)));
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "? :Sent total : %s %s",
			   buf, _GMKs (sendK));
	snprintf(buf, sizeof buf, "%7.2f", _GMKv ((receiveK)));
	sendto_one_numeric(source_p, RPL_STATSDEBUG,
			   "? :Recv total : %s %s",
			   buf, _GMKs (receiveK));

	uptime = (rb_current_time() - startup_time);
	snprintf(buf, sizeof buf, "%7.2f %s (%4.1f K/s)",
			   _GMKv (me.localClient->sendK),
			   _GMKs (me.localClient->sendK),
			   (float) ((float) me.localClient->sendK / (float) uptime));
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "? :Server send: %s", buf);
	snprintf(buf, sizeof buf, "%7.2f %s (%4.1f K/s)",
			   _GMKv (me.localClient->receiveK),
			   _GMKs (me.localClient->receiveK),
			   (float) ((float) me.localClient->receiveK / (float) uptime));
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "? :Server recv: %s", buf);
}

static inline bool
stats_l_should_show_oper(struct Client *source_p, struct Client *target_p)
{
	return SeesOper(target_p, source_p);
}

static void
stats_ltrace(struct Client *source_p, int parc, const char *parv[])
{
	bool doall = false;
	bool wilds = false;
	const char *name;
	char statchar = parv[1][0];

	if (ConfigFileEntry.stats_l_oper_only == STATS_L_OPER_ONLY_YES && !IsOperGeneral(source_p))
	{
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES, form_str(ERR_NOPRIVILEGES));
		return;
	}

	/* this is def targeted at us somehow.. */
	if (parc > 2 && !EmptyString(parv[2]))
	{
		/* directed at us generically? */
		if (match(parv[2], me.name) || (!MyClient(source_p) && !irccmp(parv[2], me.id)))
		{
			name = me.name;
			doall = true;
		}
		else
		{
			name = parv[2];
			wilds = strchr(name, '*') || strchr(name, '?');
		}

		/* must be directed at a specific person thats not us */
		if (!doall && !wilds)
		{
			struct Client *target_p;

			if (MyClient(source_p))
				target_p = find_named_person(name);
			else
				target_p = find_person(name);

			if (target_p != source_p && ConfigFileEntry.stats_l_oper_only != STATS_L_OPER_ONLY_NO
					&& !IsOperGeneral(source_p))
			{
				sendto_one_numeric(source_p, ERR_NOPRIVILEGES, form_str(ERR_NOPRIVILEGES));
			}
			else if (target_p != NULL)
			{
				stats_l_client(source_p, target_p, statchar);
			}
			else
			{
				sendto_one_numeric(source_p, ERR_NOSUCHSERVER,
						form_str(ERR_NOSUCHSERVER),
						name);
			}

			return;
		}
	}
	else
	{
		name = me.name;
		doall = true;
	}

	if (ConfigFileEntry.stats_l_oper_only != STATS_L_OPER_ONLY_NO && !IsOperGeneral(source_p))
	{
		if (doall && MyClient(source_p))
			stats_l_client(source_p, source_p, statchar);
		else
			sendto_one_numeric(source_p, ERR_NOPRIVILEGES, form_str(ERR_NOPRIVILEGES));
		return;
	}

	if (doall)
	{
		/* local opers get everyone */
		if(MyOper(source_p))
		{
			stats_l_list(source_p, name, doall, wilds, &unknown_list, statchar, NULL);
			stats_l_list(source_p, name, doall, wilds, &lclient_list, statchar, NULL);
		}
		else
		{
			/* they still need themselves if theyre local.. */
			if(MyClient(source_p))
				stats_l_client(source_p, source_p, statchar);

			stats_l_list(source_p, name, doall, wilds, &local_oper_list, statchar, stats_l_should_show_oper);
		}

		if (!ConfigServerHide.flatten_links || IsOperGeneral(source_p) ||
				IsExemptShide(source_p))
			stats_l_list(source_p, name, doall, wilds, &serv_list, statchar, NULL);

		return;
	}

	/* ok, at this point theyre looking for a specific client whos on
	 * our server.. but it contains a wildcard.  --fl
	 */
	stats_l_list(source_p, name, doall, wilds, &lclient_list, statchar, NULL);

	return;
}

static void
stats_l_list(struct Client *source_p, const char *name, bool doall, bool wilds,
	     rb_dlink_list * list, char statchar, bool (*check_fn)(struct Client *source_p, struct Client *target_p))
{
	rb_dlink_node *ptr;
	struct Client *target_p;

	/* send information about connections which match.  note, we
	 * dont need tests for IsInvisible(), because non-opers will
	 * never get here for normal clients --fl
	 */
	RB_DLINK_FOREACH(ptr, list->head)
	{
		target_p = ptr->data;

		if(!doall && wilds && !match(name, target_p->name))
			continue;

		if (check_fn == NULL || check_fn(source_p, target_p))
			stats_l_client(source_p, target_p, statchar);
	}
}

void
stats_l_client(struct Client *source_p, struct Client *target_p,
		char statchar)
{
	if(IsAnyServer(target_p))
	{
		sendto_one_numeric(source_p, RPL_STATSLINKINFO, Lformat,
				target_p->name,
				rb_linebuf_len(&target_p->localClient->buf_sendq),
				target_p->localClient->sendM,
				target_p->localClient->sendK,
				target_p->localClient->receiveM,
				target_p->localClient->receiveK,
				(int64_t)(rb_current_time() - target_p->localClient->firsttime),
				(int64_t)((rb_current_time() > target_p->localClient->lasttime) ?
				 (rb_current_time() - target_p->localClient->lasttime) : 0),
				IsOperGeneral(source_p) ? show_capabilities(target_p) : "-");
	}

	else
	{
		/* fire the doing_stats_show_idle hook to allow modules to tell us whether to show the idle time */
		hook_data_client_approval hdata_showidle;

		hdata_showidle.client = source_p;
		hdata_showidle.target = target_p;
		hdata_showidle.approved = WHOIS_IDLE_SHOW;

		call_hook(doing_stats_show_idle_hook, &hdata_showidle);
		sendto_one_numeric(source_p, RPL_STATSLINKINFO, Lformat,
				   show_ip(source_p, target_p) ?
				    (IsUpper(statchar) ?
				     get_client_name(target_p, SHOW_IP) :
				     get_client_name(target_p, HIDE_IP)) :
				    get_client_name(target_p, MASK_IP),
				    hdata_showidle.approved ? rb_linebuf_len(&target_p->localClient->buf_sendq) : 0,
				    hdata_showidle.approved ? target_p->localClient->sendM : (uint32_t)0,
				    hdata_showidle.approved ? target_p->localClient->sendK : (uint32_t)0,
				    hdata_showidle.approved ? target_p->localClient->receiveM : (uint32_t)0,
				    hdata_showidle.approved ? target_p->localClient->receiveK : (uint32_t)0,
				    (int64_t)(rb_current_time() - target_p->localClient->firsttime),
				    (int64_t)((rb_current_time() > target_p->localClient->lasttime) && hdata_showidle.approved ?
				     (rb_current_time() - target_p->localClient->lasttime) : 0),
				    "-");
	}
}

static void
rb_dump_fd_callback(int fd, const char *desc, void *data)
{
	struct Client *source_p = data;
	sendto_one_numeric(source_p, RPL_STATSDEBUG, "F :fd %-3d desc '%s'", fd, desc);
}

static void
stats_comm(struct Client *source_p)
{
	rb_dump_fd(rb_dump_fd_callback, source_p);
}
