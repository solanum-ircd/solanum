/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_nick.c: Sets a users nick.
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
#include "hash.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_stats.h"
#include "s_user.h"
#include "hash.h"
#include "whowas.h"
#include "s_serv.h"
#include "send.h"
#include "channel.h"
#include "logger.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "packet.h"
#include "scache.h"
#include "s_newconf.h"
#include "monitor.h"
#include "s_assert.h"

/* Give all UID nicks the same TS. This ensures nick TS is always the same on
 * all servers for each nick-user pair, also if a user with a UID nick changes
 * their nick but is collided again (the server detecting the collision will
 * not propagate the nick change further). -- jilles
 */
#define SAVE_NICKTS 100

static void mr_nick(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void m_nick(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void mc_nick(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void ms_nick(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void ms_uid(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void ms_euid(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void ms_save(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static bool can_save(struct Client *);
static void save_user(struct Client *, struct Client *, struct Client *);
static void bad_nickname(struct Client *, const char *);
static void change_remote_nick(struct Client *, struct Client *, time_t,
			      const char *, int);
static bool clean_username(const char *);
static bool clean_host(const char *);
static bool clean_uid(const char *uid, const char *sid);

static void set_initial_nick(struct Client *client_p, struct Client *source_p, char *nick);
static void change_local_nick(struct Client *client_p, struct Client *source_p, char *nick, int);
static void register_client(struct Client *client_p, struct Client *server,
			   const char *nick, time_t newts, int parc, const char *parv[]);
static void perform_nick_collides(struct Client *, struct Client *,
				  struct Client *, int, const char **,
				  time_t, const char *, const char *);
static void perform_nickchange_collides(struct Client *, struct Client *,
					struct Client *, int, const char **, time_t, const char *);

static int h_local_nick_change;
static int h_remote_nick_change;

struct Message nick_msgtab = {
	"NICK", 0, 0, 0, 0,
	{{mr_nick, 0}, {m_nick, 0}, {mc_nick, 3}, {ms_nick, 0}, mg_ignore, {m_nick, 0}}
};
struct Message uid_msgtab = {
	"UID", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, {ms_uid, 9}, mg_ignore, mg_ignore}
};
struct Message euid_msgtab = {
	"EUID", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, {ms_euid, 12}, mg_ignore, mg_ignore}
};
struct Message save_msgtab = {
	"SAVE", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, {ms_save, 3}, mg_ignore, mg_ignore}
};

mapi_clist_av1 nick_clist[] = { &nick_msgtab, &uid_msgtab, &euid_msgtab,
	&save_msgtab, NULL };

mapi_hlist_av1 nick_hlist[] = {
	{ "local_nick_change", &h_local_nick_change },
	{ "remote_nick_change", &h_remote_nick_change },
	{ NULL, NULL }
};

static const char nick_desc[] =
	"Provides the NICK client and server commands as well as the UID, EUID, and SAVE TS6 server commands";

DECLARE_MODULE_AV2(nick, NULL, NULL, nick_clist, nick_hlist, NULL, NULL, NULL, nick_desc);

/* mr_nick()
 *       parv[1] = nickname
 */
static void
mr_nick(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	char nick[NICKLEN];

	if (strlen(client_p->id) == 3 || (source_p->preClient && !EmptyString(source_p->preClient->id)))
	{
		exit_client(client_p, client_p, client_p, "Mixing client and server protocol");
		return;
	}

	if(parc < 2 || EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NONICKNAMEGIVEN),
			   me.name, EmptyString(source_p->name) ? "*" : source_p->name);
		return;
	}

	/* copy the nick and terminate it */
	rb_strlcpy(nick, parv[1], ConfigFileEntry.nicklen);

	/* check the nickname is ok */
	if(!clean_nick(nick, 1))
	{
		sendto_one(source_p, form_str(ERR_ERRONEUSNICKNAME),
			   me.name, EmptyString(source_p->name) ? "*" : source_p->name, parv[1]);
		return;
	}

	/* check if the nick is resv'd */
	if(find_nick_resv(nick))
	{
		sendto_one(source_p, form_str(ERR_ERRONEUSNICKNAME),
			   me.name, EmptyString(source_p->name) ? "*" : source_p->name, nick);
		return;
	}

	if(rb_dictionary_find(nd_dict, nick))
	{
		sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
			   me.name, EmptyString(source_p->name) ? "*" : source_p->name, nick);
		return;
	}

	if((target_p = find_named_client(nick)) == NULL)
		set_initial_nick(client_p, source_p, nick);
	else if(source_p == target_p)
		rb_strlcpy(source_p->name, nick, sizeof(source_p->name));
	else
		sendto_one(source_p, form_str(ERR_NICKNAMEINUSE), me.name, "*", nick);
}

/* m_nick()
 *     parv[1] = nickname
 */
static void
m_nick(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	char nick[NICKLEN];

	if(parc < 2 || EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NONICKNAMEGIVEN), me.name, source_p->name);
		return;
	}

	/* mark end of grace period, to prevent nickflooding */
	if(!IsFloodDone(source_p))
		flood_endgrace(source_p);

	/* terminate nick to NICKLEN, we dont want clean_nick() to error! */
	rb_strlcpy(nick, parv[1], ConfigFileEntry.nicklen);

	/* check the nickname is ok */
	if(!clean_nick(nick, 1))
	{
		sendto_one(source_p, form_str(ERR_ERRONEUSNICKNAME), me.name, source_p->name, nick);
		return;
	}

	if(!IsExemptResv(source_p) && find_nick_resv(nick))
	{
		sendto_one(source_p, form_str(ERR_ERRONEUSNICKNAME), me.name, source_p->name, nick);
		return;
	}

	if(rb_dictionary_find(nd_dict, nick))
	{
		sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
			   me.name, EmptyString(source_p->name) ? "*" : source_p->name, nick);
		return;
	}

	if((target_p = find_named_client(nick)))
	{
		/* If(target_p == source_p) the client is changing nicks between
		 * equivalent nicknames ie: [nick] -> {nick}
		 */
		if(target_p == source_p)
		{
			/* check the nick isnt exactly the same */
			if(strcmp(target_p->name, nick))
				change_local_nick(client_p, source_p, nick, 1);

		}

		/* drop unregged client */
		else if(IsUnknown(target_p))
		{
			exit_client(NULL, target_p, &me, "Overridden");
			change_local_nick(client_p, source_p, nick, 1);
		}
		else
			sendto_one(source_p, form_str(ERR_NICKNAMEINUSE), me.name, source_p->name, nick);

		return;
	}
	else
		change_local_nick(client_p, source_p, nick, 1);
}

/* mc_nick()
 *
 * server -> server nick change
 *    parv[1] = nickname
 *    parv[2] = TS when nick change
 */
static void
mc_nick(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	time_t newts = 0;

	/* if nicks erroneous, or too long, kill */
	if(!clean_nick(parv[1], 0))
	{
		bad_nickname(client_p, parv[1]);
		return;
	}

	newts = atol(parv[2]);
	target_p = find_named_client(parv[1]);

	/* if the nick doesnt exist, allow it and process like normal */
	if(target_p == NULL)
	{
		change_remote_nick(client_p, source_p, newts, parv[1], 1);
	}
	else if(IsUnknown(target_p))
	{
		exit_client(NULL, target_p, &me, "Overridden");
		change_remote_nick(client_p, source_p, newts, parv[1], 1);
	}
	else if(target_p == source_p)
	{
		/* client changing case of nick */
		if(strcmp(target_p->name, parv[1]))
			change_remote_nick(client_p, source_p, newts, parv[1], 1);
	}
	/* we've got a collision! */
	else
		perform_nickchange_collides(source_p, client_p, target_p,
					    parc, parv, newts, parv[1]);
}

static void
ms_nick(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	const char *nick, *server;

	nick = parc > 1 ? parv[1] : "?";
	server = parc > 7 ? parv[7] : "?";

	sendto_wallops_flags(UMODE_WALLOP, &me,
			"Link %s cancelled, TS5 nickname %s on %s introduced (old server?)",
			client_p->name, nick, server);
	sendto_server(NULL, NULL, CAP_TS6, NOCAPS,
			":%s WALLOPS :Link %s cancelled, TS5 nickname %s on %s introduced (old server?)",
			me.id, client_p->name, nick, server);
	ilog(L_SERVER, "Link %s cancelled, TS5 nickname %s on %s introduced (old server?)",
			client_p->name, nick, server);

	exit_client(client_p, client_p, &me, "TS5 nickname introduced");
}

/* ms_uid()
 *     parv[1] - nickname
 *     parv[2] - hops
 *     parv[3] - TS
 *     parv[4] - umodes
 *     parv[5] - username
 *     parv[6] - hostname
 *     parv[7] - IP
 *     parv[8] - UID
 *     parv[9] - gecos
 */
static void
ms_uid(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	time_t newts = 0;
	char squitreason[120];

	newts = atol(parv[3]);

	if(parc != 10)
	{
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				     "Dropping server %s due to (invalid) command 'UID' "
				     "with %d arguments (expecting 10)", client_p->name, parc);
		ilog(L_SERVER, "Excess parameters (%d) for command 'UID' from %s.",
		     parc, client_p->name);
		snprintf(squitreason, sizeof squitreason,
				"Excess parameters (%d) to %s command, expecting %d",
				parc, "UID", 10);
		exit_client(client_p, client_p, client_p, squitreason);
		return;
	}

	/* if nicks erroneous, or too long, kill */
	if(!clean_nick(parv[1], 0))
	{
		bad_nickname(client_p, parv[1]);
		return;
	}

	if(!clean_uid(parv[8], source_p->id))
	{
		snprintf(squitreason, sizeof squitreason,
				"Invalid UID %s for nick %s on %s/%s",
				parv[8], parv[1], source_p->name, source_p->id);
		exit_client(client_p, client_p, client_p, squitreason);
		return;
	}

	if(!clean_username(parv[5]) || !clean_host(parv[6]))
	{
		ServerStats.is_kill++;
		sendto_realops_snomask(SNO_DEBUG, L_NETWIDE,
				     "Bad user@host: %s@%s From: %s(via %s)",
				     parv[5], parv[6], source_p->name, client_p->name);
		sendto_one(client_p, ":%s KILL %s :%s (Bad user@host)", me.id, parv[8], me.name);
		return;
	}

	/* check length of clients gecos */
	if(strlen(parv[9]) > REALLEN)
	{
		char *s = LOCAL_COPY(parv[9]);
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "Long realname from server %s for %s",
				     source_p->name, parv[1]);
		s[REALLEN] = '\0';
		parv[9] = s;
	}

	target_p = find_named_client(parv[1]);

	if(target_p == NULL)
	{
		register_client(client_p, source_p, parv[1], newts, parc, parv);
	}
	else if(IsUnknown(target_p))
	{
		exit_client(NULL, target_p, &me, "Overridden");
		register_client(client_p, source_p, parv[1], newts, parc, parv);
	}
	/* we've got a collision! */
	else
		perform_nick_collides(source_p, client_p, target_p, parc, parv,
				      newts, parv[1], parv[8]);
}

/* ms_euid()
 *     parv[1] - nickname
 *     parv[2] - hops
 *     parv[3] - TS
 *     parv[4] - umodes
 *     parv[5] - username
 *     parv[6] - hostname
 *     parv[7] - IP
 *     parv[8] - UID
 *     parv[9] - realhost
 *     parv[10] - account
 *     parv[11] - gecos
 */
static void
ms_euid(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	time_t newts = 0;
	char squitreason[120];

	newts = atol(parv[3]);

	if(parc != 12)
	{
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				     "Dropping server %s due to (invalid) command 'EUID' "
				     "with %d arguments (expecting 12)", client_p->name, parc);
		ilog(L_SERVER, "Excess parameters (%d) for command 'EUID' from %s.",
		     parc, client_p->name);
		snprintf(squitreason, sizeof squitreason,
				"Excess parameters (%d) to %s command, expecting %d",
				parc, "EUID", 12);
		exit_client(client_p, client_p, client_p, squitreason);
		return;
	}

	/* if nicks erroneous, or too long, kill */
	if(!clean_nick(parv[1], 0))
	{
		bad_nickname(client_p, parv[1]);
		return;
	}

	if(!clean_uid(parv[8], source_p->id))
	{
		snprintf(squitreason, sizeof squitreason,
				"Invalid UID %s for nick %s on %s/%s",
				parv[8], parv[1], source_p->name, source_p->id);
		exit_client(client_p, client_p, client_p, squitreason);
		return;
	}

	if(!clean_username(parv[5]) || !clean_host(parv[6]))
	{
		ServerStats.is_kill++;
		sendto_realops_snomask(SNO_DEBUG, L_NETWIDE,
				     "Bad user@host: %s@%s From: %s(via %s)",
				     parv[5], parv[6], source_p->name, client_p->name);
		sendto_one(client_p, ":%s KILL %s :%s (Bad user@host)", me.id, parv[8], me.name);
		return;
	}

	if(strcmp(parv[9], "*") && !clean_host(parv[9]))
	{
		ServerStats.is_kill++;
		sendto_realops_snomask(SNO_DEBUG, L_NETWIDE,
				     "Bad realhost: %s From: %s(via %s)",
				     parv[9], source_p->name, client_p->name);
		sendto_one(client_p, ":%s KILL %s :%s (Bad user@host)", me.id, parv[8], me.name);
		return;
	}

	/* check length of clients gecos */
	if(strlen(parv[11]) > REALLEN)
	{
		char *s = LOCAL_COPY(parv[11]);
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "Long realname from server %s for %s",
				     source_p->name, parv[1]);
		s[REALLEN] = '\0';
		parv[11] = s;
	}

	target_p = find_named_client(parv[1]);

	if(target_p == NULL)
	{
		register_client(client_p, source_p, parv[1], newts, parc, parv);
	}
	else if(IsUnknown(target_p))
	{
		exit_client(NULL, target_p, &me, "Overridden");
		register_client(client_p, source_p, parv[1], newts, parc, parv);
	}
	/* we've got a collision! */
	else
		perform_nick_collides(source_p, client_p, target_p, parc, parv,
				      newts, parv[1], parv[8]);
}

/* ms_save()
 *   parv[1] - UID
 *   parv[2] - TS
 */
static void
ms_save(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;

	target_p = find_id(parv[1]);
	if (target_p == NULL)
		return;
	if (!IsPerson(target_p))
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				"Ignored SAVE message for non-person %s from %s",
				target_p->name, source_p->name);
	else if (IsDigit(target_p->name[0]))
		sendto_realops_snomask(SNO_DEBUG, L_ALL,
				"Ignored noop SAVE message for %s from %s",
				target_p->name, source_p->name);
	else if (target_p->tsinfo == atol(parv[2]))
		save_user(client_p, source_p, target_p);
	else
		sendto_realops_snomask(SNO_SKILL, L_ALL,
				"Ignored SAVE message for %s from %s",
				target_p->name, source_p->name);
}

/* clean_username()
 *
 * input	- username to check
 * output	- false if erroneous, else true
 * side effects -
 */
static bool
clean_username(const char *username)
{
	int len = 0;

	for(; *username; username++)
	{
		len++;

		if(!IsUserChar(*username))
			return false;
	}

	if(len > USERLEN)
		return false;

	return true;
}

/* clean_host()
 *
 * input	- host to check
 * output	- false if erroneous, else true
 * side effects -
 */
static bool
clean_host(const char *host)
{
	int len = 0;

	for(; *host; host++)
	{
		len++;

		if(!IsHostChar(*host))
			return false;
	}

	if(len > HOSTLEN)
		return false;

	return true;
}

static bool
clean_uid(const char *uid, const char *sid)
{
	int len = 1;

	if(strncmp(uid, sid, strlen(sid)))
		return false;

	if(!IsDigit(*uid++))
		return false;

	for(; *uid; uid++)
	{
		len++;

		if(!IsIdChar(*uid))
			return false;
	}

	if(len != IDLEN - 1)
		return false;

	return true;
}

static void
set_initial_nick(struct Client *client_p, struct Client *source_p, char *nick)
{
	char note[NICKLEN + 10];

	/* This had to be copied here to avoid problems.. */
	source_p->tsinfo = rb_current_time();
	if(source_p->name[0])
		del_from_client_hash(source_p->name, source_p);

	rb_strlcpy(source_p->name, nick, sizeof(source_p->name));
	add_to_client_hash(nick, source_p);

	snprintf(note, sizeof(note), "Nick: %s", nick);
	rb_note(client_p->localClient->F, note);

	if(source_p->flags & FLAGS_SENTUSER)
	{
		/* got user, heres nick. */
		register_local_user(client_p, source_p);
	}
}

static void
change_local_nick(struct Client *client_p, struct Client *source_p,
		char *nick, int dosend)
{
	struct Channel *chptr;
	char note[NICKLEN + 10];
	int samenick;
	hook_cdata hook_info;

	if (dosend)
	{
		chptr = find_bannickchange_channel(source_p);
		if (chptr != NULL)
		{
			sendto_one_numeric(source_p, ERR_BANNICKCHANGE,
					form_str(ERR_BANNICKCHANGE),
					nick, chptr->chname);
			return;
		}
		if((source_p->localClient->last_nick_change + ConfigFileEntry.max_nick_time) < rb_current_time())
			source_p->localClient->number_of_nick_changes = 0;

		source_p->localClient->last_nick_change = rb_current_time();
		source_p->localClient->number_of_nick_changes++;

		if(ConfigFileEntry.anti_nick_flood && !IsOperGeneral(source_p) &&
				source_p->localClient->number_of_nick_changes > ConfigFileEntry.max_nick_changes)
		{
			sendto_one(source_p, form_str(ERR_NICKTOOFAST),
					me.name, source_p->name, source_p->name,
					nick, ConfigFileEntry.max_nick_time);
			return;
		}
	}

	samenick = irccmp(source_p->name, nick) ? 0 : 1;

	/* dont reset TS if theyre just changing case of nick */
	if(!samenick)
	{
		/* force the TS to increase -- jilles */
		if (source_p->tsinfo >= rb_current_time())
			source_p->tsinfo++;
		else
			source_p->tsinfo = rb_current_time();
		monitor_signoff(source_p);
		/* we only do bancache for local users -- jilles */
		if(source_p->user)
			invalidate_bancache_user(source_p);
	}

	hook_info.client = source_p;
	hook_info.arg1 = source_p->name;
	hook_info.arg2 = nick;
	call_hook(h_local_nick_change, &hook_info);

	sendto_realops_snomask(SNO_NCHANGE, L_ALL,
			     "Nick change: From %s to %s [%s@%s]",
			     source_p->name, nick, source_p->username, source_p->host);

	/* send the nick change to the users channels */
	sendto_common_channels_local(source_p, NOCAPS, NOCAPS, ":%s!%s@%s NICK :%s",
				     source_p->name, source_p->username, source_p->host, nick);

	/* send the nick change to servers.. */
	if(source_p->user)
	{
		whowas_add_history(source_p, 1);

		if (dosend)
		{
			sendto_server(client_p, NULL, CAP_TS6, NOCAPS, ":%s NICK %s :%ld",
					use_id(source_p), nick, (long) source_p->tsinfo);
		}
	}

	/* Finally, add to hash */
	del_from_client_hash(source_p->name, source_p);
	rb_strlcpy(source_p->name, nick, sizeof(source_p->name));
	add_to_client_hash(nick, source_p);

	if(!samenick)
		monitor_signon(source_p);

	/* Make sure everyone that has this client on its accept list
	 * loses that reference.
	 */
	del_all_accepts(source_p, false);

	snprintf(note, sizeof(note), "Nick: %s", nick);
	rb_note(client_p->localClient->F, note);

	return;
}

/*
 * change_remote_nick()
 */
static void
change_remote_nick(struct Client *client_p, struct Client *source_p,
		   time_t newts, const char *nick, int dosend)
{
	struct Client *target_p;
	rb_dlink_node *ptr, *next_ptr;
	struct nd_entry *nd;
	int samenick = irccmp(source_p->name, nick) ? 0 : 1;
	hook_cdata hook_info;

	/* client changing their nick - dont reset ts if its same */
	if(!samenick)
	{
		source_p->tsinfo = newts ? newts : rb_current_time();
		monitor_signoff(source_p);
	}

	hook_info.client = source_p;
	hook_info.arg1 = source_p->name;
	hook_info.arg2 = nick;
	call_hook(h_remote_nick_change, &hook_info);

	sendto_common_channels_local(source_p, NOCAPS, NOCAPS, ":%s!%s@%s NICK :%s",
				     source_p->name, source_p->username, source_p->host, nick);

	if(source_p->user)
	{
		whowas_add_history(source_p, 1);
		if (dosend)
		{
			sendto_server(client_p, NULL, CAP_TS6, NOCAPS, ":%s NICK %s :%ld",
					use_id(source_p), nick, (long) source_p->tsinfo);
		}
	}

	del_from_client_hash(source_p->name, source_p);

	/* invalidate nick delay when a remote client uses the nick.. */
	if((nd = rb_dictionary_retrieve(nd_dict, nick)))
		free_nd_entry(nd);

	rb_strlcpy(source_p->name, nick, sizeof(source_p->name));
	add_to_client_hash(nick, source_p);

	if(!samenick)
		monitor_signon(source_p);

	/* remove all accepts pointing to the client */
	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, source_p->on_allow_list.head)
	{
		target_p = ptr->data;

		if (!has_common_channel(source_p, target_p))
		{
			rb_dlinkFindDestroy(source_p, &target_p->localClient->allow_list);
			rb_dlinkDestroy(ptr, &source_p->on_allow_list);
		}
	}
}

static void
perform_nick_collides(struct Client *source_p, struct Client *client_p,
		      struct Client *target_p, int parc, const char *parv[],
		      time_t newts, const char *nick, const char *uid)
{
	int sameuser;
	int use_save;
	const char *action;

	use_save = ConfigFileEntry.collision_fnc && can_save(target_p) &&
		uid != NULL && can_save(source_p);
	action = use_save ? "saved" : "killed";

	/* if we dont have a ts, or their TS's are the same, kill both */
	if(!newts || !target_p->tsinfo || (newts == target_p->tsinfo))
	{
		sendto_realops_snomask(SNO_SKILL, L_NETWIDE,
				     "Nick collision on %s(%s <- %s)(both %s)",
				     target_p->name, target_p->from->name, client_p->name, action);

		if (use_save)
		{
			save_user(&me, &me, target_p);
			ServerStats.is_save++;
			sendto_one(client_p, ":%s SAVE %s %ld", me.id,
					uid, (long)newts);
			register_client(client_p, source_p,
					uid, SAVE_NICKTS, parc, parv);
		}
		else
		{
			sendto_one_numeric(target_p, ERR_NICKCOLLISION,
					form_str(ERR_NICKCOLLISION), target_p->name);

			/* if the new client being introduced has a UID, we need to
			 * issue a KILL for it..
			 */
			if(uid)
				sendto_one(client_p, ":%s KILL %s :%s (Nick collision (new))",
						me.id, uid, me.name);

			/* we then need to KILL the old client everywhere */
			kill_client_serv_butone(NULL, target_p, "%s (Nick collision (new))", me.name);
			ServerStats.is_kill++;

			target_p->flags |= FLAGS_KILLED;
			exit_client(client_p, target_p, &me, "Nick collision (new)");
		}
		return;
	}
	/* the timestamps are different */
	else
	{
		sameuser = (target_p->user) && !irccmp(target_p->username, parv[5])
			&& !irccmp(target_p->host, parv[6]);

		if((sameuser && newts < target_p->tsinfo) ||
		   (!sameuser && newts > target_p->tsinfo))
		{
			/* if we have a UID, then we need to issue a KILL,
			 * otherwise we do nothing and hope that the other
			 * client will collide it..
			 */
			if (use_save)
			{
				sendto_one(client_p, ":%s SAVE %s %ld", me.id,
						uid, (long)newts);
				register_client(client_p, source_p,
						uid, SAVE_NICKTS, parc, parv);
			}
			else if(uid)
				sendto_one(client_p,
					   ":%s KILL %s :%s (Nick collision (new))",
					   me.id, uid, me.name);
			return;
		}
		else
		{
			if(sameuser)
				sendto_realops_snomask(SNO_SKILL, L_NETWIDE,
						     "Nick collision on %s(%s <- %s)(older %s)",
						     target_p->name, target_p->from->name,
						     client_p->name, action);
			else
				sendto_realops_snomask(SNO_SKILL, L_NETWIDE,
						     "Nick collision on %s(%s <- %s)(newer %s)",
						     target_p->name, target_p->from->name,
						     client_p->name, action);

			if (use_save)
			{
				ServerStats.is_save++;
				save_user(&me, &me, target_p);
			}
			else
			{
				ServerStats.is_kill++;
				sendto_one_numeric(target_p, ERR_NICKCOLLISION,
						form_str(ERR_NICKCOLLISION), target_p->name);

				/* now we just need to kill the existing client */
				kill_client_serv_butone(client_p, target_p,
						"%s (Nick collision (new))", me.name);

				target_p->flags |= FLAGS_KILLED;
				(void) exit_client(client_p, target_p, &me, "Nick collision");
			}

			register_client(client_p, source_p,
					nick, newts, parc, parv);
		}
	}
}


static void
perform_nickchange_collides(struct Client *source_p, struct Client *client_p,
			    struct Client *target_p, int parc,
			    const char *parv[], time_t newts, const char *nick)
{
	int sameuser;
	int use_save;
	const char *action;

	use_save = ConfigFileEntry.collision_fnc && can_save(target_p) &&
		can_save(source_p);
	action = use_save ? "saved" : "killed";

	/* its a client changing nick and causing a collide */
	if(!newts || !target_p->tsinfo || (newts == target_p->tsinfo) || !source_p->user)
	{
		sendto_realops_snomask(SNO_SKILL, L_NETWIDE,
				     "Nick change collision from %s to %s(%s <- %s)(both %s)",
				     source_p->name, target_p->name, target_p->from->name,
				     client_p->name, action);

		if (use_save)
		{
			ServerStats.is_save += 2;
			save_user(&me, &me, target_p);
			sendto_one(client_p, ":%s SAVE %s %ld", me.id,
					source_p->id, (long)newts);
			/* don't send a redundant nick change */
			if (!IsDigit(source_p->name[0]))
				change_remote_nick(client_p, source_p, SAVE_NICKTS, source_p->id, 1);
		}
		else
		{
			ServerStats.is_kill++;
			sendto_one_numeric(target_p, ERR_NICKCOLLISION,
					form_str(ERR_NICKCOLLISION), target_p->name);

			kill_client_serv_butone(NULL, source_p, "%s (Nick change collision)", me.name);

			ServerStats.is_kill++;

			kill_client_serv_butone(NULL, target_p, "%s (Nick change collision)", me.name);

			target_p->flags |= FLAGS_KILLED;
			exit_client(NULL, target_p, &me, "Nick collision(new)");
			source_p->flags |= FLAGS_KILLED;
			exit_client(client_p, source_p, &me, "Nick collision(old)");
		}
		return;
	}
	else
	{
		sameuser = !irccmp(target_p->username, source_p->username) &&
			!irccmp(target_p->host, source_p->host);

		if((sameuser && newts < target_p->tsinfo) ||
		   (!sameuser && newts > target_p->tsinfo))
		{
			if(sameuser)
				sendto_realops_snomask(SNO_SKILL, L_NETWIDE,
						     "Nick change collision from %s to %s(%s <- %s)(older %s)",
						     source_p->name, target_p->name,
						     target_p->from->name, client_p->name, action);
			else
				sendto_realops_snomask(SNO_SKILL, L_NETWIDE,
						     "Nick change collision from %s to %s(%s <- %s)(newer %s)",
						     source_p->name, target_p->name,
						     target_p->from->name, client_p->name, action);

			if (use_save)
			{
				ServerStats.is_save++;
				/* can't broadcast a SAVE because the
				 * nickchange has happened at client_p
				 * but not in other directions -- jilles */
				sendto_one(client_p, ":%s SAVE %s %ld", me.id,
						source_p->id, (long)newts);
				/* send a :<id> NICK <id> <ts> (!) */
				if (!IsDigit(source_p->name[0]))
					change_remote_nick(client_p, source_p, SAVE_NICKTS, source_p->id, 1);
			}
			else
			{
				ServerStats.is_kill++;

				sendto_one_numeric(source_p, ERR_NICKCOLLISION,
						form_str(ERR_NICKCOLLISION), source_p->name);

				/* kill the client issuing the nickchange */
				kill_client_serv_butone(client_p, source_p,
						"%s (Nick change collision)", me.name);

				source_p->flags |= FLAGS_KILLED;

				if(sameuser)
					exit_client(client_p, source_p, &me, "Nick collision(old)");
				else
					exit_client(client_p, source_p, &me, "Nick collision(new)");
			}
			return;
		}
		else
		{
			if(sameuser)
				sendto_realops_snomask(SNO_SKILL, L_NETWIDE,
						     "Nick collision on %s(%s <- %s)(older %s)",
						     target_p->name, target_p->from->name,
						     client_p->name, action);
			else
				sendto_realops_snomask(SNO_SKILL, L_NETWIDE,
						     "Nick collision on %s(%s <- %s)(newer %s)",
						     target_p->name, target_p->from->name,
						     client_p->name, action);

			if (use_save)
			{
				ServerStats.is_save++;
				save_user(&me, &me, target_p);
			}
			else
			{
				sendto_one_numeric(target_p, ERR_NICKCOLLISION,
						form_str(ERR_NICKCOLLISION), target_p->name);

				/* kill the client who existed before hand */
				kill_client_serv_butone(client_p, target_p, "%s (Nick collision)", me.name);

				ServerStats.is_kill++;

				target_p->flags |= FLAGS_KILLED;
				(void) exit_client(client_p, target_p, &me, "Nick collision");
			}
		}
	}

	change_remote_nick(client_p, source_p, newts, nick, 1);
}

static void
register_client(struct Client *client_p, struct Client *server,
		const char *nick, time_t newts, int parc, const char *parv[])
{
	struct Client *source_p;
	struct User *user;
	struct nd_entry *nd;
	const char *m;
	int flag;

	source_p = make_client(client_p);
	user = make_user(source_p);
	rb_dlinkAddTail(source_p, &source_p->node, &global_client_list);

	source_p->hopcount = atoi(parv[2]);
	source_p->tsinfo = newts;

	rb_strlcpy(source_p->name, nick, sizeof(source_p->name));
	rb_strlcpy(source_p->username, parv[5], sizeof(source_p->username));
	rb_strlcpy(source_p->host, parv[6], sizeof(source_p->host));
	rb_strlcpy(source_p->orighost, source_p->host, sizeof(source_p->orighost));

	if(parc == 12)
	{
		rb_strlcpy(source_p->info, parv[11], sizeof(source_p->info));
		rb_strlcpy(source_p->sockhost, parv[7], sizeof(source_p->sockhost));
		rb_strlcpy(source_p->id, parv[8], sizeof(source_p->id));
		add_to_id_hash(source_p->id, source_p);
		if (strcmp(parv[9], "*"))
		{
			rb_strlcpy(source_p->orighost, parv[9], sizeof(source_p->orighost));
			if (irccmp(source_p->host, source_p->orighost))
				SetDynSpoof(source_p);
		}
		if (strcmp(parv[10], "*"))
			rb_strlcpy(source_p->user->suser, parv[10], sizeof(source_p->user->suser));
	}
	else if(parc == 10)
	{
		rb_strlcpy(source_p->info, parv[9], sizeof(source_p->info));
		rb_strlcpy(source_p->sockhost, parv[7], sizeof(source_p->sockhost));
		rb_strlcpy(source_p->id, parv[8], sizeof(source_p->id));
		add_to_id_hash(source_p->id, source_p);
	}
	else
	{
		s_assert(0);
	}

	/* remove any nd entries for this nick */
	if((nd = rb_dictionary_retrieve(nd_dict, nick)))
		free_nd_entry(nd);

	add_to_client_hash(nick, source_p);
	add_to_hostname_hash(source_p->orighost, source_p);
	monitor_signon(source_p);

	m = &parv[4][1];
	while(*m)
	{
		flag = user_modes[(unsigned char) *m];

		if(flag & UMODE_SERVICE)
		{
			int hit = 0;
			rb_dlink_node *ptr;

			RB_DLINK_FOREACH(ptr, service_list.head)
			{
				if(!irccmp((const char *) ptr->data, server->name))
				{
					hit++;
					break;
				}
			}

			if(!hit)
			{
				m++;
				continue;
			}
		}

		/* increment +i count if theyre invis */
		if(!(source_p->umodes & UMODE_INVISIBLE) && (flag & UMODE_INVISIBLE))
			Count.invisi++;

		/* increment opered count if theyre opered */
		if(!(source_p->umodes & UMODE_OPER) && (flag & UMODE_OPER))
			Count.oper++;

		source_p->umodes |= flag;
		m++;
	}

	if(IsOper(source_p) && !IsService(source_p))
		rb_dlinkAddAlloc(source_p, &oper_list);

	SetRemoteClient(source_p);

	if(++Count.total > Count.max_tot)
		Count.max_tot = Count.total;

	source_p->servptr = server;

	rb_dlinkAdd(source_p, &source_p->lnode, &source_p->servptr->serv->users);

	call_hook(h_new_remote_user, source_p);

	introduce_client(client_p, source_p, user, nick, parc == 12);
}

/* Check if we can do SAVE. target_p can be a client to save or a
 * server introducing a client -- jilles */
static bool
can_save(struct Client *target_p)
{
	struct Client *serv_p;

	if (MyClient(target_p))
		return true;
	if (!has_id(target_p))
		return false;
	serv_p = IsServer(target_p) ? target_p : target_p->servptr;
	while (serv_p != NULL && serv_p != &me)
	{
		if (!(serv_p->serv->caps & CAP_SAVE))
			return false;
		serv_p = serv_p->servptr;
	}
	return serv_p == &me;
}

static void
save_user(struct Client *client_p, struct Client *source_p,
		struct Client *target_p)
{
	if (!MyConnect(target_p) && (!has_id(target_p) || !IsCapable(target_p->from, CAP_SAVE)))
	{
		/* This shouldn't happen */
		/* Note we only need SAVE support in this direction */
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				"Killed %s!%s@%s for nick collision detected by %s (%s does not support SAVE)",
				target_p->name, target_p->username, target_p->host, source_p->name, target_p->from->name);
		kill_client_serv_butone(NULL, target_p, "%s (Nick collision (no SAVE support))", me.name);
		ServerStats.is_kill++;

		target_p->flags |= FLAGS_KILLED;
		(void) exit_client(NULL, target_p, &me, "Nick collision (no SAVE support)");
		return;
	}
	sendto_server(client_p, NULL, CAP_SAVE|CAP_TS6, NOCAPS, ":%s SAVE %s %ld",
			source_p->id, target_p->id, (long)target_p->tsinfo);
	sendto_server(client_p, NULL, CAP_TS6, CAP_SAVE, ":%s NICK %s :%ld",
			target_p->id, target_p->id, (long)SAVE_NICKTS);
	if (!IsMe(client_p))
		sendto_realops_snomask(SNO_SKILL, L_ALL,
				"Received SAVE message for %s from %s",
				target_p->name, source_p->name);
	if (MyClient(target_p))
	{
		sendto_one_numeric(target_p, RPL_SAVENICK,
				form_str(RPL_SAVENICK), target_p->id);
		change_local_nick(target_p, target_p, target_p->id, 0);
		target_p->tsinfo = SAVE_NICKTS;
	}
	else
		change_remote_nick(target_p, target_p, SAVE_NICKTS, target_p->id, 0);
}

static void bad_nickname(struct Client *client_p, const char *nick)
{
	char squitreason[100];

	sendto_wallops_flags(UMODE_WALLOP, &me,
			"Squitting %s because of bad nickname %s (NICKLEN mismatch?)",
			client_p->name, nick);
	sendto_server(NULL, NULL, CAP_TS6, NOCAPS,
			":%s WALLOPS :Squitting %s because of bad nickname %s (NICKLEN mismatch?)",
			me.id, client_p->name, nick);
	ilog(L_SERVER, "Link %s cancelled, bad nickname %s sent (NICKLEN mismatch?)",
			client_p->name, nick);

	snprintf(squitreason, sizeof squitreason,
			"Bad nickname introduced [%s]", nick);
	exit_client(client_p, client_p, &me, squitreason);
}
