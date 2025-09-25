/* modules/m_services.c
 *   Copyright (C) 2005 Lee Hardy <lee -at- leeh.co.uk>
 *   Copyright (C) 2005 ircd-ratbox development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "stdinc.h"

#include "send.h"
#include "channel.h"
#include "client.h"
#include "defaults.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_serv.h"
#include "hash.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "whowas.h"
#include "monitor.h"
#include "supported.h"

static const char services_desc[] = "Provides support for running a services daemon";

static int _modinit(void);
static void _moddeinit(void);

static void mark_services(void);
static void unmark_services(void);

static void me_su(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void me_login(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void me_rsfnc(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void me_nickdelay(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

static void h_svc_server_introduced(void *);
static void h_svc_whois(void *);
static void h_svc_stats(void *);
static void h_svc_conf_read_start(void *);
static void h_svc_conf_read_end(void *);

struct Message su_msgtab = {
	"SU", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_su, 2}, mg_ignore}
};
struct Message login_msgtab = {
	"LOGIN", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_login, 2}, mg_ignore}
};
struct Message rsfnc_msgtab = {
	"RSFNC", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_rsfnc, 4}, mg_ignore}
};
struct Message nickdelay_msgtab = {
	"NICKDELAY", 0, 0, 0, 0,
	{mg_unreg, mg_ignore, mg_ignore, mg_ignore, {me_nickdelay, 3}, mg_ignore}
};

mapi_clist_av1 services_clist[] = {
	&su_msgtab, &login_msgtab, &rsfnc_msgtab, &nickdelay_msgtab, NULL
};
mapi_hfn_list_av1 services_hfnlist[] = {
	{ "doing_stats",	h_svc_stats },
	{ "doing_whois",	h_svc_whois },
	{ "doing_whois_global",	h_svc_whois },
	{ "server_introduced",	h_svc_server_introduced },
	{ "conf_read_start", h_svc_conf_read_start },
	{ "conf_read_end", h_svc_conf_read_end },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(services, _modinit, _moddeinit, services_clist, NULL, services_hfnlist, NULL, NULL, services_desc);

static int
_modinit(void)
{
	mark_services();
	add_isupport("FNC", isupport_string, "");
	return 0;
}

static void
_moddeinit(void)
{
	delete_isupport("FNC");
	unmark_services();
}

static void
me_su(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p,
	int parc, const char *parv[])
{
	struct Client *target_p;

	if(!(source_p->flags & FLAGS_SERVICE))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
			"Non-service server %s attempting to execute services-only command SU", source_p->name);
		return;
	}

	if((target_p = find_client(parv[1])) == NULL)
		return;

	if(!target_p->user)
		return;

	if(EmptyString(parv[2]))
		target_p->user->suser[0] = '\0';
	else
		rb_strlcpy(target_p->user->suser, parv[2], sizeof(target_p->user->suser));

	sendto_common_channels_local(target_p, CLICAP_ACCOUNT_NOTIFY, NOCAPS, ":%s!%s@%s ACCOUNT %s",
					    target_p->name, target_p->username, target_p->host,
					    EmptyString(target_p->user->suser) ? "*" : target_p->user->suser);
	struct monitor *monptr = find_monitor(target_p->name, 0);
	if(monptr)
		sendto_monitor_with_capability_butserial(target_p, monptr, CLICAP_EXTENDED_MONITOR | CLICAP_ACCOUNT_NOTIFY, NOCAPS, true, ":%s!%s@%s ACCOUNT %s",
					    target_p->name, target_p->username, target_p->host,
					    EmptyString(target_p->user->suser) ? "*" : target_p->user->suser);

	if (MyClient(target_p))
	{
		if (EmptyString(target_p->user->suser))
	                sendto_one(target_p, form_str(RPL_LOGGEDOUT), me.name, target_p->name,
				target_p->name, target_p->username, target_p->host);
		else
			sendto_one(target_p, form_str(RPL_LOGGEDIN), me.name, target_p->name,
				target_p->name, target_p->username, target_p->host, parv[2], parv[2]);
	}

	invalidate_bancache_user(target_p);
}

static void
me_login(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p,
	int parc, const char *parv[])
{
	if(!IsPerson(source_p))
		return;

	rb_strlcpy(source_p->user->suser, parv[1], sizeof(source_p->user->suser));
}

/* me_rsfnc()
 *     parv[1] = current user nickname
 *     parv[2] = target nickname
 *     parv[3] = new nickts
 *     parv[4] = current nickts
 *     parv[5] = optional; 0 (don't override RESVs) or 1 (override RESVs)
 */
static void
me_rsfnc(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p,
	int parc, const char *parv[])
{
	struct Client *target_p;
	struct Client *exist_p;
	time_t newts, curts;
	struct nd_entry *nd;
	char note[NAMELEN + 10];

	if(!(source_p->flags & FLAGS_SERVICE))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
			"Non-service server %s attempting to execute services-only command RSFNC", source_p->name);
		return;
	}

	if((target_p = find_person(parv[1])) == NULL)
		return;

	if(!MyClient(target_p))
		return;

	if(!clean_nick(parv[2], 0) || IsDigit(parv[2][0]))
		return;

	curts = atol(parv[4]);

	/* if tsinfo is different from what it was when services issued the
	 * RSFNC, then we ignore it.  This can happen when a client changes
	 * nicknames before the RSFNC arrives.. --anfl
	 */
	if(target_p->tsinfo != curts)
		return;

	/* received a non-forced RSFNC for a nickname that is RESV.
	 * silently ignore it. ~jess
	 */
	if(parc > 5 && atoi(parv[5]) == 0 && find_nick_resv(parv[2]))
		return;

	if((exist_p = find_named_client(parv[2])))
	{
		char buf[BUFSIZE];

		/* this would be one hell of a race condition to trigger
		 * this one given the tsinfo check above, but its here for
		 * safety --anfl
		 */
		if(target_p == exist_p)
			goto doit;

		if(MyClient(exist_p))
			sendto_one(exist_p, ":%s KILL %s :(Nickname regained by services)",
				me.name, exist_p->name);

		exist_p->flags |= FLAGS_KILLED;
		/* Do not send kills to servers for unknowns -- jilles */
		if(IsClient(exist_p))
		{
			kill_client_serv_butone(NULL, exist_p, "%s (Nickname regained by services)",
						me.name);
			sendto_realops_snomask(SNO_SKILL, L_ALL,
					"Nick collision due to services forced nick change on %s",
					parv[2]);
		}

		snprintf(buf, sizeof(buf), "Killed (%s (Nickname regained by services))",
			me.name);
		exit_client(NULL, exist_p, &me, buf);
	}

doit:
	newts = atol(parv[3]);

	/* timestamp is older than 15mins, ignore it */
	if(newts < (rb_current_time() - 900))
		newts = rb_current_time() - 900;

	target_p->tsinfo = newts;

	monitor_signoff(target_p);

	invalidate_bancache_user(target_p);

	sendto_realops_snomask(SNO_NCHANGE, L_ALL,
			"Nick change: From %s to %s [%s@%s]",
			target_p->name, parv[2], target_p->username,
			target_p->host);

	sendto_common_channels_local(target_p, NOCAPS, NOCAPS, ":%s!%s@%s NICK :%s",
				target_p->name, target_p->username,
				target_p->host, parv[2]);

	whowas_add_history(target_p, 1);
	sendto_server(NULL, NULL, CAP_TS6, NOCAPS, ":%s NICK %s :%ld",
			use_id(target_p), parv[2], (long) target_p->tsinfo);

	del_from_client_hash(target_p->name, target_p);

	/* invalidate nick delay because we're forcing this nick to be used */
	nd = rb_dictionary_retrieve(nd_dict, parv[2]);
	if (nd != NULL)
		free_nd_entry(nd);

	rb_strlcpy(target_p->name, parv[2], NICKLEN);
	add_to_client_hash(target_p->name, target_p);

	monitor_signon(target_p);

	/* Make sure everyone that has this client on its accept list
	 * loses that reference.
	 */
	del_all_accepts(target_p, false);

	snprintf(note, sizeof(note), "Nick: %s", target_p->name);
	rb_note(target_p->localClient->F, note);
}

/*
** me_nickdelay
**      parv[1] = duration in seconds (0 to remove)
**      parv[2] = nick
*/
static void
me_nickdelay(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	int duration;
	struct nd_entry *nd;

	if(!(source_p->flags & FLAGS_SERVICE))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
			"Non-service server %s attempting to execute services-only command NICKDELAY", source_p->name);
		return;
	}

	duration = atoi(parv[1]);
	if (duration <= 0)
	{
		nd = rb_dictionary_retrieve(nd_dict, parv[2]);
		if (nd != NULL)
			free_nd_entry(nd);
	}
	else
	{
		if (duration > 86400)
			duration = 86400;
		add_nd_entry(parv[2]);
		nd = rb_dictionary_retrieve(nd_dict, parv[2]);
		if (nd != NULL)
			nd->expire = rb_current_time() + duration;
	}
}

static void
h_svc_server_introduced(void *data)
{
	hook_data_client *hdata = data;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, service_list.head)
	{
		if(!irccmp((const char *) ptr->data, hdata->target->name))
		{
			hdata->target->flags |= FLAGS_SERVICE;
			return;
		}
	}
}

static void
h_svc_whois(void *data_)
{
	hook_data_client *data = data_;
	char *p = data->target->user->suser;
	if(!EmptyString(p))
	{
		/* Try to strip off any leading digits as this may be used to
		 * store both an ID number and an account name in one field.
		 * If only digits are present, leave as is.
		 */
		while(IsDigit(*p))
			p++;
		if(*p == '\0')
			p = data->target->user->suser;

		sendto_one_numeric(data->client, RPL_WHOISLOGGEDIN,
				form_str(RPL_WHOISLOGGEDIN),
				data->target->name, p);
	}
}

static void
h_svc_stats(void *data_)
{
	hook_data_int *data = data_;
	char statchar = (char) data->arg2;
	rb_dlink_node *ptr;

	if (statchar == 'U' && IsOperGeneral(data->client))
	{
		RB_DLINK_FOREACH(ptr, service_list.head)
		{
			sendto_one_numeric(data->client, RPL_STATSULINE,
						form_str(RPL_STATSULINE),
						(const char *)ptr->data, "*", "*", "s");
		}
	}
}

static void
h_svc_conf_read_start(void *dummy)
{
	unmark_services();
}

static void
unmark_services(void)
{
	struct Client *target_p;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, global_serv_list.head)
	{
		target_p = ptr->data;

		target_p->flags &= ~FLAGS_SERVICE;
	}
}

static void
h_svc_conf_read_end(void *dummy)
{
	mark_services();
}

static void
mark_services(void)
{
	struct Client *target_p;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, service_list.head)
	{
		target_p = find_server(NULL, (const char *)ptr->data);

		if (target_p)
			target_p->flags |= FLAGS_SERVICE;
	}
}
