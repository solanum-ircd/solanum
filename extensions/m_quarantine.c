/*
 * Solanum: a slightly advanced ircd
 * m_quarantine.c: restrict connections until they identify to services
 *
 * Copyright (c) 2026 Ryan Schmidt <skizzerz@skizzerz.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "stdinc.h"
#include "channel.h"
#include "chmode.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "logger.h"
#include "newconf.h"
#include "numeric.h"
#include "send.h"
#include "s_newconf.h"
#include "s_serv.h"
#include "s_user.h"

#define IsOperQuarantine(x) (HasPrivilege((x), "oper:quarantine"))
#define IsQuarantined(x) ((x)->umodes & user_modes['q'])
#define DEFAULT_JOIN_REASON "Cannot join channel (+q) - you need to be logged into your NickServ account"
#define DEFAULT_MSG_REASON "Cannot send to nick/channel (+q) - you need to be logged into your NickServ account"
#define DEFAULT_OTHER_MSG_REASON "they are quarantined and will be unable to respond to you"
#define DEFAULT_APPLY_MSG "You have been quarantined and must log into your NickServ account before you can join channels. Please see /STATS p for assistance."
#define DEFAULT_REMOVE_MSG "You are no longer quarantined and can freely join channels."

static const char quarantine_desc[] = "QUARANTINE system and umode +q to restrict unidentified connections.";
static char *join_reason = NULL;
static char *msg_reason = NULL;
static char *other_msg_reason = NULL;
static char *apply_msg = NULL;
static char *remove_msg = NULL;
static int part_channels = 1;
static char **allowed_channels = NULL;

static struct Client *allowed_umode_change = NULL;

static int modinit(void);
static void moddeinit(void);
static void me_quarantine(struct MsgBuf *, struct Client *, struct Client *, int, const char *[]);
static void mo_quarantine(struct MsgBuf *, struct Client *, struct Client *, int, const char *[]);
static void me_unquarantine(struct MsgBuf *, struct Client *, struct Client *, int, const char *[]);
static void mo_unquarantine(struct MsgBuf *, struct Client *, struct Client *, int, const char *[]);
static void add_quarantine(struct Client *source_p, struct Client *target_p, const char *reason);
static void remove_quarantine(struct Client *source_p, struct Client *target_p);
static void quarantine_account_change(void *);
static void quarantine_change_umode(void *);
static void quarantine_conf_info(void *);
static void quarantine_default_umode_begin(void *);
static void quarantine_default_umode_end(void *);
static void quarantine_join_channel(void *);
static void quarantine_privmsg_channel(void *);
static void quarantine_privmsg_user(void *);
static void quarantine_reset_conf(void *);
static void quarantine_set_allow_channels(void *);

struct Message quarantine_msgtab = {
	"QUARANTINE", 0, 0, 0, 0,
	{ mg_unreg, mg_not_oper, mg_not_oper, mg_ignore, { me_quarantine, 2 }, { mo_quarantine, 2 } }
};

struct Message unquarantine_msgtab = {
	"UNQUARANTINE", 0, 0, 0, 0,
	{ mg_unreg, mg_not_oper, mg_not_oper, mg_ignore, { me_unquarantine, 1 }, { mo_unquarantine, 1 } }
};

struct ConfEntry conf_quarantine_table[] = {
	{ "allow_channels", CF_QSTRING | CF_FLIST, quarantine_set_allow_channels, 0, NULL },
	{ "apply_msg", CF_QSTRING, NULL, BUFSIZE, &apply_msg },
	{ "join_reason", CF_QSTRING, NULL, BUFSIZE, &join_reason },
	{ "msg_reason", CF_QSTRING, NULL, BUFSIZE, &msg_reason },
	{ "other_msg_reason", CF_QSTRING, NULL, BUFSIZE, &other_msg_reason },
	{ "part_channels", CF_YESNO, NULL, 0, &part_channels },
	{ "remove_msg", CF_QSTRING, NULL, BUFSIZE, &remove_msg },
	{ "\0",	0, NULL, 0, NULL }
};

mapi_clist_av1 quarantine_clist[] = { &quarantine_msgtab, &unquarantine_msgtab, NULL };

mapi_hfn_list_av1 quarantine_hfn_list[] = {
	{ "account_change", quarantine_account_change },
	{ "can_join", quarantine_join_channel },
	{ "conf_read_start", quarantine_reset_conf },
	{ "doing_info_conf", quarantine_conf_info },
	{ "introduce_client", quarantine_default_umode_end },
	{ "new_local_user", quarantine_default_umode_begin, HOOK_MONITOR },
	{ "privmsg_channel", quarantine_privmsg_channel },
	{ "privmsg_user", quarantine_privmsg_user },
	{ "umode_changed", quarantine_change_umode },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(m_quarantine, modinit, moddeinit, quarantine_clist, NULL, quarantine_hfn_list, NULL, NULL, quarantine_desc);

static int
modinit(void)
{
	if (find_top_conf("quarantine") != NULL)
	{
		ierror("m_quarantine: a top conf block named quarantine already exists, unloading extension");
		return -1;
	}

	user_modes['q'] = find_umode_slot();
	if (!user_modes['q'])
	{
		ierror("m_quarantine: unable to allocate usermode slot for +q, unloading extension");
		return -1;
	}

	construct_umodebuf();
	add_top_conf("quarantine", NULL, NULL, conf_quarantine_table);
	return 0;
}

static void
moddeinit(void)
{
	user_modes['q'] = 0;
	construct_umodebuf();
	remove_top_conf("quarantine");
	quarantine_reset_conf(NULL);
}

static void
mo_quarantine(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if (!IsOperQuarantine(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "quarantine");
		return;
	}

	if (parc < 3 || EmptyString(parv[1]) || EmptyString(parv[2]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "QUARANTINE");
		return;
	}

	struct Client *target_p = find_named_person(parv[1]);
	if (target_p == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), parv[1]);
		return;
	}

	if (MyClient(target_p))
		add_quarantine(source_p, target_p, parv[2]);
	else
		sendto_one(target_p, ":%s ENCAP %s QUARANTINE %s :%s",
			get_id(source_p, target_p), target_p->servptr->name, get_id(target_p, target_p), parv[2]);
}

static void
me_quarantine(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if (parc < 3 || EmptyString(parv[1]) || EmptyString(parv[2]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "QUARANTINE");
		return;
	}

	struct Client *target_p = find_named_person(parv[1]);
	if (target_p == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), parv[1]);
		return;
	}

	if (!MyClient(target_p))
		return;

	add_quarantine(source_p, target_p, parv[2]);
}

static void
mo_unquarantine(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if (!IsOperQuarantine(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "quarantine");
		return;
	}

	if (parc < 2 || EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "UNQUARANTINE");
		return;
	}

	struct Client *target_p = find_named_person(parv[1]);
	if (target_p == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), parv[1]);
		return;
	}

	if (MyClient(target_p))
		remove_quarantine(source_p, target_p);
	else
		sendto_one(target_p, ":%s ENCAP %s UNQUARANTINE %s",
			get_id(source_p, target_p), target_p->servptr->name, get_id(target_p, target_p));
}

static void
me_unquarantine(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if (parc < 2 || EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "UNQUARANTINE");
		return;
	}

	struct Client *target_p = find_named_person(parv[1]);
	if (target_p == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), parv[1]);
		return;
	}

	if (!MyClient(target_p))
		return;

	remove_quarantine(source_p, target_p);
}

static void
add_quarantine(struct Client *source_p, struct Client *target_p, const char *reason)
{
	const char *parv[4] = { target_p->name, target_p->name, "+q", NULL };

	if (IsQuarantined(target_p))
	{
		sendto_one_notice(source_p, ":*** %s is already quarantined.",
			target_p->name);
		return;
	}

	if (!EmptyString(target_p->user->suser))
	{
		sendto_one_notice(source_p, ":*** %s is already logged into services and cannot be quarantined.",
			target_p->name);
		return;
	}

	if (IsOper(target_p))
	{
		sendto_one_notice(source_p, ":*** %s is an oper and cannot be put into quarantine.",
			target_p->name);
		return;
	}

	if (IsService(target_p))
	{
		sendto_one_notice(source_p, ":*** %s is a network service and cannot be put into quarantine.",
			target_p->name);
		return;
	}

	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s has put %s into QUARANTINE [%s]",
		source_p->name, target_p->name, reason);
	sendto_one_notice(target_p, ":*** %s",
		EmptyString(apply_msg) ? DEFAULT_APPLY_MSG : apply_msg);

	allowed_umode_change = target_p;
	user_mode(target_p, target_p, 3, parv);
	allowed_umode_change = NULL;

	if (part_channels)
	{
		rb_dlink_node *ptr, *next_ptr;
		RB_DLINK_FOREACH_SAFE(ptr, next_ptr, target_p->user->channel.head)
		{
			struct membership *msptr = ptr->data;
			struct Channel *chptr = msptr->chptr;
			bool allowed = false;
			if (allowed_channels != NULL)
			{
				for (int i = 0; allowed_channels[i] != NULL; i++)
				{
					if (!irccmp(chptr->chname, allowed_channels[i]))
					{
						allowed = true;
						break;
					}
				}
			}

			if (!allowed)
			{
				sendto_server(NULL, chptr, CAP_TS6, NOCAPS, ":%s PART %s",
					use_id(source_p), chptr->chname);
				sendto_channel_local(target_p, ALL_MEMBERS, chptr, ":%s!%s@%s PART %s",
					target_p->name, target_p->username, target_p->host, chptr->chname);
				remove_user_from_channel(msptr);
			}
		}
	}
}

static void
remove_quarantine(struct Client *source_p, struct Client *target_p)
{
	const char *parv[4] = { target_p->name, target_p->name, "-q", NULL };

	if (!IsQuarantined(target_p))
	{
		sendto_one_notice(source_p, ":*** %s is not currently quarantined.",
			target_p->name);
		return;
	}

	if (source_p != target_p)
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s has removed %s from QUARANTINE",
			source_p->name, target_p->name);
	sendto_one_notice(target_p, ":*** %s",
		EmptyString(remove_msg) ? DEFAULT_REMOVE_MSG : remove_msg);

	allowed_umode_change = target_p;
	user_mode(target_p, target_p, 3, parv);
	allowed_umode_change = NULL;
}

static void
quarantine_account_change(void *data_)
{
	hook_cdata *data = data_;
	if (!MyClient(data->client) || !IsQuarantined(data->client))
		return;

	if (!EmptyString(data->client->user->suser))
		remove_quarantine(data->client, data->client);
}

static void
quarantine_change_umode(void *data_)
{
	hook_data_umode_changed *data = data_;

	if (!MyClient(data->client))
		return;

	/* If someone successfully opers, unquarantine them.
	 * Services might be down and we don't want to impede them doing what they need to do.
	 */
	if (IsOper(data->client) && IsQuarantined(data->client))
	{
		sendto_one_notice(data->client, ":*** %s",
			EmptyString(remove_msg) ? DEFAULT_REMOVE_MSG : remove_msg);
		data->client->umodes &= ~user_modes['q'];
		return;
	}

	bool quarantine_changed = ((data->client->umodes ^ data->oldumodes) & user_modes['q']) == user_modes['q'];
	if (data->client != allowed_umode_change && quarantine_changed)
	{
		if (data->oldumodes & user_modes['q'])
			data->client->umodes |= user_modes['q'];
		else
			data->client->umodes &= ~user_modes['q'];
	}
}

static void
quarantine_conf_info(void *data_)
{
	hook_data *data = data_;

	/* quarantine::allow_channels not listed, since INFO in general doesn't seem
	 * to support sending list-type conf options (e.g. no hidden_caps) */

	sendto_one(data->client, ":%s %d %s :%-30s %-16s [%s]",
		get_id(&me, data->client), RPL_INFO,
		get_id(data->client, data->client),
		"quarantine::apply_msg",
		EmptyString(apply_msg) ? DEFAULT_APPLY_MSG : apply_msg,
		"Notice sent to users that have been quarantined");

	sendto_one(data->client, ":%s %d %s :%-30s %-16s [%s]",
		get_id(&me, data->client), RPL_INFO,
		get_id(data->client, data->client),
		"quarantine::join_reason",
		EmptyString(join_reason) ? DEFAULT_JOIN_REASON : join_reason,
		"Sent to +q users when JOIN is denied");

	sendto_one(data->client, ":%s %d %s :%-30s %-16s [%s]",
		get_id(&me, data->client), RPL_INFO,
		get_id(data->client, data->client),
		"quarantine::msg_reason",
		EmptyString(msg_reason) ? DEFAULT_MSG_REASON : msg_reason,
		"Sent to +q users when sending a message is denied");

	sendto_one(data->client, ":%s %d %s :%-30s %-16s [%s]",
		get_id(&me, data->client), RPL_INFO,
		get_id(data->client, data->client),
		"quarantine::other_msg_reason",
		EmptyString(other_msg_reason) ? DEFAULT_OTHER_MSG_REASON : other_msg_reason,
		"Sent to users trying to message a +q user");

	sendto_one(data->client, ":%s %d %s :%-30s %-16s [%s]",
		get_id(&me, data->client), RPL_INFO,
		get_id(data->client, data->client),
		"quarantine::part_channels",
		part_channels ? "YES" : "NO",
		"QUARANTINE causes user to part disallowed channels");

	sendto_one(data->client, ":%s %d %s :%-30s %-16s [%s]",
		get_id(&me, data->client), RPL_INFO,
		get_id(data->client, data->client),
		"quarantine::remove_msg",
		EmptyString(remove_msg) ? DEFAULT_REMOVE_MSG : remove_msg,
		"Notice sent to users that are no longer quarantined");
}

static void
quarantine_default_umode_begin(void *data)
{
	/* if ircd.conf has +q as a default umode, and the user didn't identify
	 * during registration (via SASL or PASS), allow the +q and notify the user
	 */
	struct Client *source_p = data;
	if (!IsAnyDead(source_p) && IsQuarantined(source_p))
	{
		if (!EmptyString(source_p->user->suser))
			source_p->umodes &= ~user_modes['q'];
		else
		{
			sendto_one_notice(source_p, ":*** %s",
				EmptyString(apply_msg) ? DEFAULT_APPLY_MSG : apply_msg);
			allowed_umode_change = source_p;
		}
	}
}

static void
quarantine_default_umode_end(void *data)
{
	/* Introducing a new local user calls three hooks in the following order:
	 * 1. new_local_user
	 * 2. umode_changed
	 * 3. introduce_client
	 * We make sure we clear the allowed_umode_change after introduce_client so it can't linger
	 * after the client is introduced. Non-local users also call the latter two, but effectively
	 * no-ops (umode_changed aborts if non-local, this just sets something already NULL to NULL),
	 * so no issues there
	 */
	allowed_umode_change = NULL;
}

static void
quarantine_join_channel(void *data_)
{
	hook_data_channel *data = data_;
	if (!MyClient(data->client) || !IsQuarantined(data->client))
		return;

	/* if some other hook function already rejected this join attempt, keep that rejection */
	if (data->approved != 0)
		return;

	if (allowed_channels != NULL)
	{
		for (int i = 0; allowed_channels[i] != NULL; i++)
		{
			if (!irccmp(data->chptr->chname, allowed_channels[i]))
				return;
		}
	}

	/* Remind the user that they're quarantined */
	sendto_one_numeric(data->client, ERR_NEEDREGGEDNICK, "%s :%s",
		data->chptr->chname,
		EmptyString(join_reason) ? DEFAULT_JOIN_REASON : join_reason);
	data->approved = ERR_CUSTOM;
}

static void
quarantine_privmsg_channel(void *data_)
{
	hook_data_privmsg_channel *data = data_;
	if (!MyClient(data->source_p) || !IsQuarantined(data->source_p))
		return;

	/* if some other hook function already rejected this attempt, keep that rejection */
	if (data->approved != 0)
		return;

	if (allowed_channels != NULL)
	{
		for (int i = 0; allowed_channels[i] != NULL; i++)
		{
			if (!irccmp(data->chptr->chname, allowed_channels[i]))
				return;
		}
	}

	data->approved = ERR_CANNOTSENDTOCHAN;

	/* Don't give error messages for TAGMSG since many clients autogenerate these (e.g. +typing) */
	if (data->msgtype != MESSAGE_TYPE_TAGMSG)
		sendto_one_numeric(data->source_p, ERR_CANNOTSENDTOCHAN, "%s :%s",
			data->chptr->chname,
			EmptyString(msg_reason) ? DEFAULT_MSG_REASON : msg_reason);
}

static void
quarantine_privmsg_user(void *data_)
{
	hook_data_privmsg_user *data = data_;
	if (!MyClient(data->source_p))
		return;

	/* if some other hook function already rejected this attempt, keep that rejection */
	if (data->approved != 0)
		return;

	if (!IsQuarantined(data->source_p))
	{
		if (IsQuarantined(data->target_p) && !IsOper(data->source_p) && !IsService(data->source_p))
		{
			if (data->msgtype != MESSAGE_TYPE_TAGMSG)
				sendto_one_numeric(data->source_p, ERR_CANNOTSENDTOUSER, form_str(ERR_CANNOTSENDTOUSER),
					data->target_p->name,
					EmptyString(other_msg_reason) ? DEFAULT_OTHER_MSG_REASON : other_msg_reason);
			data->approved = ERR_CANNOTSENDTOUSER;
		}
		return;
	}

	/* let quarantined users message opers and services */
	if (IsOper(data->target_p) || IsService(data->target_p))
		return;

	data->approved = ERR_CANNOTSENDTOCHAN;

	/* Don't give error messages for TAGMSG since many clients autogenerate these (e.g. +typing) */
	if (data->msgtype != MESSAGE_TYPE_TAGMSG)
		sendto_one_numeric(data->source_p, ERR_CANNOTSENDTOCHAN, "%s :%s",
			data->target_p->name,
			EmptyString(msg_reason) ? DEFAULT_MSG_REASON : msg_reason);
}

static void
quarantine_reset_conf(void *unused)
{
	if (allowed_channels != NULL)
	{
		for (int i = 0; allowed_channels[i] != NULL; i++)
			rb_free(allowed_channels[i]);
		rb_free(allowed_channels);
		allowed_channels = NULL;
	}

	if (apply_msg != NULL)
		rb_free(apply_msg);

	if (join_reason != NULL)
		rb_free(join_reason);

	if (msg_reason != NULL)
		rb_free(msg_reason);

	if (other_msg_reason != NULL)
		rb_free(other_msg_reason);

	if (remove_msg != NULL)
		rb_free(remove_msg);
}

static void
quarantine_set_allow_channels(void *data)
{
	size_t n = 0;
	for (conf_parm_t *arg = data; arg; arg = arg->next)
		n++;

	allowed_channels = rb_malloc((n + 1) * sizeof(char *));

	n = 0;
	for (conf_parm_t *arg = data; arg; arg = arg->next)
		allowed_channels[n++] = rb_strdup(arg->v.string);
	allowed_channels[n] = NULL;
}
