/*
 * modules/um_callerid.c
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "hash.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"
#include "privilege.h"
#include "s_newconf.h"
#include "hook.h"
#include "supported.h"
#include "logger.h"

#define IsSetStrictCallerID(c)	((c->umodes & user_modes['g']) == user_modes['g'])
#define IsSetRelaxedCallerID(c)	((c->umodes & user_modes['G']) == user_modes['G'])
#define IsSetAnyCallerID(c)	(IsSetStrictCallerID(c) || IsSetRelaxedCallerID(c))
#define IsSetTalkThroughCallerID(c)	((c->umodes & user_modes['M']) == user_modes['M'])

static const char um_callerid_desc[] =
	"Provides usermodes +g and +G which restrict messages from unauthorized users.";


struct CallerIDOverrideSession {
	rb_dlink_node node;

	struct Client *client;
	time_t deadline;
};

static rb_dlink_list callerid_overriding_opers = { NULL, NULL, 0 };
struct ev_entry *expire_callerid_override_deadlines_ev = NULL;

static void
update_session_deadline(struct Client *source_p)
{
	struct CallerIDOverrideSession *session_p = NULL;
	rb_dlink_node *n;

	RB_DLINK_FOREACH(n, callerid_overriding_opers.head)
	{
		struct CallerIDOverrideSession *s = n->data;

		if (s->client == source_p)
		{
			session_p = s;
			break;
		}
	}

	if (session_p != NULL)
	{
		rb_dlinkDelete(&session_p->node, &callerid_overriding_opers);
	}
	else
	{
		session_p = rb_malloc(sizeof(struct CallerIDOverrideSession));
		session_p->client = source_p;
	}

	session_p->deadline = rb_current_time() + 1800;

	rb_dlinkAddTail(session_p, &session_p->node, &callerid_overriding_opers);
}

static void
expire_callerid_override_deadlines(void *unused)
{
	rb_dlink_node *n, *tn;

	RB_DLINK_FOREACH_SAFE(n, tn, callerid_overriding_opers.head)
	{
		struct CallerIDOverrideSession *session_p = n->data;

		if (session_p->deadline >= rb_current_time())
		{
			break;
		}
		else
		{
			const char *parv[4] = {session_p->client->name, session_p->client->name, "-M", NULL};
			user_mode(session_p->client, session_p->client, 3, parv);
		}
	}
}

static bool
allow_message(struct Client *source_p, struct Client *target_p)
{
	if (!MyClient(target_p))
		return true;

	if (!IsSetAnyCallerID(target_p))
		return true;

	if (!IsPerson(source_p))
		return true;

	if (IsSetRelaxedCallerID(target_p) &&
			!IsSetStrictCallerID(target_p) &&
			has_common_channel(source_p, target_p))
		return true;

	/* XXX: controversial?  allow opers to send through +g */
	if (IsSetTalkThroughCallerID(source_p) || MayHavePrivilege(source_p, "oper:always_message"))
		return true;

	if (accept_message(source_p, target_p))
		return true;

	return false;
}

static void
send_callerid_notice(enum message_type msgtype, struct Client *source_p, struct Client *target_p)
{
	if (!MyClient(target_p))
		return;

	if (msgtype == MESSAGE_TYPE_NOTICE)
		return;

	sendto_one_numeric(source_p, ERR_TARGUMODEG, form_str(ERR_TARGUMODEG),
		target_p->name, IsSetStrictCallerID(target_p) ? "+g" : "+G");

	if ((target_p->localClient->last_caller_id_time + ConfigFileEntry.caller_id_wait) < rb_current_time())
	{
		sendto_one_numeric(source_p, RPL_TARGNOTIFY, form_str(RPL_TARGNOTIFY),
			target_p->name);

		sendto_one(target_p, form_str(RPL_UMODEGMSG),
			   me.name, target_p->name, source_p->name,
			   source_p->username, source_p->host, IsSetStrictCallerID(target_p) ? "+g" : "+G");

		target_p->localClient->last_caller_id_time = rb_current_time();
	}
}

static bool
add_callerid_accept_for_source(enum message_type msgtype, struct Client *source_p, struct Client *target_p)
{
	/* only do this on source_p's server */
	if (!MyClient(source_p))
		return true;

	/*
	 * XXX: Controversial? Allow target users to send replies
	 * through a +g.  Rationale is that people can presently use +g
	 * as a way to taunt users, e.g. harass them and hide behind +g
	 * as a way of griefing.  --nenolod
	 */
	if(msgtype != MESSAGE_TYPE_NOTICE &&
		IsSetAnyCallerID(source_p) &&
		!accept_message(target_p, source_p) &&
		!IsOperGeneral(target_p))
	{
		if(rb_dlink_list_length(&source_p->localClient->allow_list) <
				(unsigned long)ConfigFileEntry.max_accept)
		{
			rb_dlinkAddAlloc(target_p, &source_p->localClient->allow_list);
			rb_dlinkAddAlloc(source_p, &target_p->on_allow_list);
		}
		else
		{
			sendto_one_numeric(source_p, ERR_OWNMODE,
					form_str(ERR_OWNMODE),
					target_p->name, IsSetStrictCallerID(target_p) ? "+g" : "+G");
			return false;
		}
	}

	return true;
}

static void
h_hdl_invite(void *vdata)
{
	hook_data_channel_approval *data = vdata;
	struct Client *source_p = data->client;
	struct Client *target_p = data->target;
	static char errorbuf[BUFSIZE];

	if (data->approved)
		return;

	if (!add_callerid_accept_for_source(MESSAGE_TYPE_PRIVMSG, source_p, target_p))
	{
		data->approved = ERR_TARGUMODEG;
		return;
	}

	if (allow_message(source_p, target_p))
		return;

	snprintf(errorbuf, sizeof errorbuf, form_str(ERR_TARGUMODEG),
		 target_p->name, IsSetStrictCallerID(target_p) ? "+g" : "+G");

	data->approved = ERR_TARGUMODEG;
	data->error = errorbuf;
}

static void
h_hdl_privmsg_user(void *vdata)
{
	hook_data_privmsg_user *data = vdata;
	enum message_type msgtype = data->msgtype;
	struct Client *source_p = data->source_p;
	struct Client *target_p = data->target_p;

	if (data->approved)
		return;

	if (!add_callerid_accept_for_source(msgtype, source_p, target_p))
	{
		data->approved = ERR_TARGUMODEG;
		return;
	}

	if (allow_message(source_p, target_p))
		return;

	send_callerid_notice(msgtype, source_p, target_p);

	data->approved = ERR_TARGUMODEG;
}

static void
check_umode_change(void *vdata)
{
	hook_data_umode_changed *data = (hook_data_umode_changed *)vdata;
	bool changed = false;
	struct Client *source_p = data->client;

	if (!MyClient(source_p))
		return;

	if (data->oldumodes & UMODE_OPER && !IsOper(source_p))
		source_p->umodes &= ~user_modes['M'];

	changed = ((data->oldumodes ^ source_p->umodes) & user_modes['M']);

	if (changed && source_p->umodes & user_modes['M'])
	{
		if (!HasPrivilege(source_p, "oper:message"))
		{
			sendto_one_notice(source_p, ":*** You need oper:message privilege for +M");
			source_p->umodes &= ~user_modes['M'];
			return;
		}

		update_session_deadline(source_p);
	}
	else if (changed)
	{
		// Unsetting +M; remove the timeout session
		rb_dlink_node *n, *tn;

		RB_DLINK_FOREACH_SAFE(n, tn, callerid_overriding_opers.head)
		{
			struct CallerIDOverrideSession *session_p = n->data;

			if (session_p->client != source_p)
				continue;

			rb_dlinkDelete(n, &callerid_overriding_opers);
			rb_free(session_p);
		}
	}
}

static void check_priv_change(void *vdata)
{
	hook_data_priv_change *data = (hook_data_priv_change*)vdata;
	struct Client *source_p = data->client;
	const char *fakeparv[4];

	if (!MyClient(source_p))
		return;

	if (source_p->umodes & user_modes['M'] && !HasPrivilege(source_p, "oper:message"))
	{
		sendto_one_notice(source_p, ":*** You need oper:message privilege for +M");
		fakeparv[0] = fakeparv[1] = source_p->name;
		fakeparv[2] = "-M";
		fakeparv[3] = NULL;
		user_mode(source_p, source_p, 3, fakeparv);
	}
}

static void
handle_client_exit(void *vdata)
{
	hook_data_client_exit *data = (hook_data_client_exit *) vdata;
	rb_dlink_node *n, *tn;
	struct Client *source_p = data->target;

	RB_DLINK_FOREACH_SAFE(n, tn, callerid_overriding_opers.head)
	{
		struct CallerIDOverrideSession *session_p = n->data;

		if (session_p->client != source_p)
			continue;

		rb_dlinkDelete(n, &callerid_overriding_opers);
		rb_free(session_p);
	}
}

static mapi_hfn_list_av1 um_callerid_hfnlist[] = {
	{ "umode_changed", check_umode_change },
	{ "priv_change", check_priv_change },
	{ "invite", h_hdl_invite },
	{ "privmsg_user", h_hdl_privmsg_user },
	{ "client_exit", handle_client_exit },
	{ NULL, NULL }
};

static int
um_callerid_modinit(void)
{
	rb_dlink_node *ptr;

	user_modes['g'] = find_umode_slot();
	if (!user_modes['g'])
	{
		ierror("um_callerid: unable to allocate usermode slot for +g; unloading module.");
		return -1;
	}

	user_modes['G'] = find_umode_slot();
	if (!user_modes['G'])
	{
		user_modes['g'] = 0;

		ierror("um_callerid: unable to allocate usermode slot for +G; unloading module.");
		return -1;
	}

	user_modes['M'] = find_umode_slot();
	if (!user_modes['M'])
	{
		user_modes['g'] = 0;
		user_modes['G'] = 0;

		ierror("um_callerid: unable to allocate usermode slot for +M; unloading module.");
		return -1;
	}

	construct_umodebuf();

	add_isupport("CALLERID", isupport_umode, "g");

	RB_DLINK_FOREACH(ptr, lclient_list.head)
	{
		struct Client *client_p = ptr->data;
		if (IsPerson(client_p) && (client_p->umodes & user_modes['M']))
			update_session_deadline(client_p);
	}

	expire_callerid_override_deadlines_ev = rb_event_add("expire_callerid_override_deadlines", expire_callerid_override_deadlines, NULL, 60);

	return 0;
}

static void
um_callerid_moddeinit(void)
{
	user_modes['g'] = 0;
	user_modes['G'] = 0;
	user_modes['M'] = 0;
	construct_umodebuf();

	delete_isupport("CALLERID");
}

DECLARE_MODULE_AV2(um_callerid, um_callerid_modinit, um_callerid_moddeinit,
		   NULL, NULL, um_callerid_hfnlist, NULL, NULL, um_callerid_desc);
