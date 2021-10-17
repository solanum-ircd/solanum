/*
 * Helpops system.
 *   -- kaniini
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_newconf.h"
#include "numeric.h"

static const char helpops_desc[] = "The helpops system as used by freenode";

static rb_dlink_list helper_list = { NULL, NULL, 0 };
static void h_hdl_stats_request(void *hdata);
static void h_hdl_new_remote_user(void *client_p);
static void h_hdl_client_exit(void *hdata);
static void h_hdl_umode_changed(void *hdata);
static void h_hdl_whois(void *hdata);
static void recurse_client_exit(struct Client *client_p);
static void helper_add(struct Client *client_p);
static void helper_delete(struct Client *client_p);
static void mo_dehelper(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void me_dehelper(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void do_dehelper(struct Client *source_p, struct Client *target_p);

mapi_hfn_list_av1 helpops_hfnlist[] = {
	{ "doing_stats", h_hdl_stats_request },
	{ "new_remote_user", h_hdl_new_remote_user },
	{ "client_exit", h_hdl_client_exit },
	{ "umode_changed", h_hdl_umode_changed },
	{ "doing_whois", h_hdl_whois },
	{ "doing_whois_global", h_hdl_whois },
	{ NULL, NULL }
};

#define UMODECHAR_HELPOPS 'h'

struct Message dehelper_msgtab = {
	"DEHELPER", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_not_oper, mg_ignore, {me_dehelper, 2}, {mo_dehelper, 2}}
};

mapi_clist_av1 helpops_clist[] = { &dehelper_msgtab, NULL };

static void
mo_dehelper(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	struct Client *target_p;

	if (!HasPrivilege(source_p, "oper:dehelper"))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "dehelper");
		return;
	}

	if(!(target_p = find_named_person(parv[1])))
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), parv[1]);
		return;
	}

	if(MyClient(target_p))
		do_dehelper(source_p, target_p);
	else
		sendto_one(target_p, ":%s ENCAP %s DEHELPER %s",
				use_id(source_p), target_p->servptr->name, use_id(target_p));
}

static void
me_dehelper(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	struct Client *target_p = find_person(parv[1]);
	if(!target_p)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), parv[1]);
		return;
	}
	if(!MyClient(target_p))
		return;

	do_dehelper(source_p, target_p);
}

static void
do_dehelper(struct Client *source_p, struct Client *target_p)
{
	const char *fakeparv[4];
	static const char minus_helpops[3] = {'-', UMODECHAR_HELPOPS, '\0'};

	if(!(target_p->umodes & user_modes[UMODECHAR_HELPOPS]))
		return;

	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s is using DEHELPER on %s",
			source_p->name, target_p->name);
	sendto_one_notice(target_p, ":*** %s is using DEHELPER on you", source_p->name);

	fakeparv[0] = fakeparv[1] = target_p->name;
	fakeparv[2] = minus_helpops;
	fakeparv[3] = NULL;
	user_mode(target_p, target_p, 3, fakeparv);
}

static int
_modinit(void)
{
	rb_dlink_node *ptr;

	user_modes[UMODECHAR_HELPOPS] = find_umode_slot();
	construct_umodebuf();

	RB_DLINK_FOREACH (ptr, global_client_list.head)
	{
		struct Client *client_p = ptr->data;
		if (IsPerson(client_p) && (client_p->umodes & user_modes[UMODECHAR_HELPOPS]))
			helper_add(client_p);
	}

	return 0;
}

static void
_moddeinit(void)
{
	rb_dlink_node *n, *tn;

	user_modes[UMODECHAR_HELPOPS] = 0;
	construct_umodebuf();

	RB_DLINK_FOREACH_SAFE(n, tn, helper_list.head)
		rb_dlinkDestroy(n, &helper_list);
}

static void
h_hdl_stats_request(void *data)
{
	hook_data_int *hdata = data;
	struct Client *target_p;
	rb_dlink_node *helper_ptr;
	unsigned int count = 0;

	if (hdata->arg2 != 'p')
		return;

	RB_DLINK_FOREACH (helper_ptr, helper_list.head)
	{
		target_p = helper_ptr->data;

		if(target_p->user->away)
			continue;

		count++;

		if (IsOper(hdata->client) && SeesOper(target_p, hdata->client)
				&& !EmptyString(target_p->user->opername))
			sendto_one_numeric(hdata->client, RPL_STATSDEBUG,
				"p :%s (%s@%s) {%s}",
				target_p->name, target_p->username, target_p->host,
				target_p->user->opername);
		else
			sendto_one_numeric(hdata->client, RPL_STATSDEBUG,
				"p :%s (%s@%s)",
				target_p->name, target_p->username, target_p->host);
	}

	sendto_one_numeric(hdata->client, RPL_STATSDEBUG,
		"p :%u staff members", count);

	hdata->result = 1;
}

static void
helper_add(struct Client *client_p)
{
	if (rb_dlinkFind(client_p, &helper_list) != NULL)
		return;

	rb_dlinkAddAlloc(client_p, &helper_list);
}

static void
helper_delete(struct Client *client_p)
{
	rb_dlinkFindDestroy(client_p, &helper_list);
}

static void
h_hdl_new_remote_user(void *data)
{
	struct Client *client_p = data;
	if (client_p->umodes & user_modes[UMODECHAR_HELPOPS])
		helper_add(client_p);
}

static void
recurse_client_exit(struct Client *client_p)
{
	if (IsPerson(client_p))
	{
		if (client_p->umodes & user_modes[UMODECHAR_HELPOPS])
			helper_delete(client_p);
	}
	else if (IsServer(client_p))
	{
		rb_dlink_node *nptr;

		RB_DLINK_FOREACH(nptr, client_p->serv->users.head)
			recurse_client_exit(nptr->data);

		RB_DLINK_FOREACH(nptr, client_p->serv->servers.head)
			recurse_client_exit(nptr->data);
	}
}

static void
h_hdl_client_exit(void *data)
{
	hook_data_client_exit *hdata = data;
	recurse_client_exit(hdata->target);
}

static void
h_hdl_umode_changed(void *data)
{
	hook_data_umode_changed *hdata = data;
	struct Client *source_p = hdata->client;

	/* didn't change +h umode, we don't need to do anything */
	bool changed = (hdata->oldumodes ^ source_p->umodes) & user_modes[UMODECHAR_HELPOPS];

	if (source_p->umodes & user_modes[UMODECHAR_HELPOPS])
	{
		if (MyClient(source_p) && !HasPrivilege(source_p, "usermode:helpops"))
		{
			source_p->umodes &= ~user_modes[UMODECHAR_HELPOPS];
			sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "usermode:helpops");
			/* they didn't ask for +h so we must be removing it */
			if (!changed)
				helper_delete(source_p);
			return;
		}

		if (changed)
			helper_add(source_p);
	}
	else if (changed)
	{
		helper_delete(source_p);
	}
}

static void
h_hdl_whois(void *data)
{
	hook_data_client *hdata = data;
	struct Client *source_p = hdata->client;
	struct Client *target_p = hdata->target;

	if ((target_p->umodes & user_modes[UMODECHAR_HELPOPS]) && EmptyString(target_p->user->away))
	{
		sendto_one_numeric(source_p, RPL_WHOISHELPOP, form_str(RPL_WHOISHELPOP), target_p->name);
	}
}

DECLARE_MODULE_AV2(helpops, _modinit, _moddeinit, helpops_clist, NULL, helpops_hfnlist, NULL, NULL, helpops_desc);
