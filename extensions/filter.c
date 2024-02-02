/*
 *  ircd-ratbox: A slightly useful ircd.
 *  filter.c: Drop messages we don't like
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
 *
 *  $Id$
 */

#include "stdinc.h"
#include "channel.h"
#include "client.h"
#include "chmode.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "send.h"
#include "s_newconf.h"
#include "s_serv.h"
#include "s_user.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "operhash.h"
#include "inline/stringops.h"
#include "msgbuf.h"
#include "hostmask.h"
#include "s_conf.h"

#include <hs_common.h>
#include <hs_runtime.h>

#define FILTER_NICK     0
#define FILTER_USER     0
#define FILTER_HOST     0

#define FILTER_EXIT_MSG "Connection closed"

static const char filter_desc[] = "Filter messages using a precompiled Hyperscan database";

static void filter_msg_user(void *data);
static void filter_msg_channel(void *data);
static void filter_client_quit(void *data);
static void filter_client_nick_set(void *data);
static void filter_client_nick_change(void *data);
static void on_client_exit(void *data);

static void mo_setfilter(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void me_setfilter(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

static char *filter_data = NULL;
static size_t filter_data_len = 0;
static hs_database_t *filter_db;
static hs_scratch_t *filter_scratch;

static int filter_enable = 1;

static const char *cmdname[MESSAGE_TYPE_COUNT] = {
	[MESSAGE_TYPE_PRIVMSG] = "PRIVMSG",
	[MESSAGE_TYPE_NOTICE] = "NOTICE",
	[MESSAGE_TYPE_PART] = "PART",
};

enum filter_state {
	FILTER_EMPTY,
	FILTER_FILLING,
	FILTER_LOADED
};

#define ACT_DROP  (1 << 0)
#define ACT_KILL  (1 << 1)
#define ACT_ALARM (1 << 2)

static enum filter_state state = FILTER_EMPTY;
static char check_str[21] = "";

static unsigned filter_chmode, filter_umode;

mapi_hfn_list_av1 filter_hfnlist[] = {
	{ "privmsg_user", filter_msg_user },
	{ "privmsg_channel", filter_msg_channel },
	{ "client_quit", filter_client_quit },
	{ "local_nick_set_approve", filter_client_nick_set },
	{ "local_nick_change_approve", filter_client_nick_change },
	{ "client_exit", on_client_exit },
	{ NULL, NULL }
};


struct Message setfilter_msgtab = {
	"SETFILTER", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, {me_setfilter, 2}, {mo_setfilter, 2}}
};

static int
modinit(void)
{
	filter_umode = user_modes['u'] = find_umode_slot();
	construct_umodebuf();
	filter_chmode = cflag_add('u', chm_simple);
	return 0;
}

static void
moddeinit(void)
{
	if (filter_umode) {
		user_modes['u'] = 0;
		construct_umodebuf();
	}
	if (filter_chmode)
		cflag_orphan('u');
	if (filter_scratch)
		hs_free_scratch(filter_scratch);
	if (filter_db)
		hs_free_database(filter_db);
	if (filter_data)
		rb_free(filter_data);
}


mapi_clist_av1 filter_clist[] = { &setfilter_msgtab, NULL };

DECLARE_MODULE_AV2(filter, modinit, moddeinit, filter_clist, NULL, filter_hfnlist, NULL, "0.4", filter_desc);

static int
setfilter(const char *check, const char *data, const char **error)
{
	if (error) *error = "unknown";

	if (!strcasecmp(data, "disable")) {
		filter_enable = 0;
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
			"Filtering disabled.");
		return 0;
	}
	if (!strcasecmp(data, "enable")) {
		filter_enable = 1;
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
			"Filtering enabled.");
		return 0;
	}

	if (strlen(check) > sizeof check_str - 1) {
		if (error) *error = "check string too long";
		return -1;
	}

	if (!strcasecmp(data, "new")) {
		if (state == FILTER_FILLING) {
			rb_free(filter_data);
			filter_data = 0;
			filter_data_len = 0;
		}
		state = FILTER_FILLING;
		strcpy(check_str, check);
		return 0;
	}

	if (!strcasecmp(data, "drop")) {
		if (!filter_db) {
			if (error) *error = "no database to drop";
			return -1;
		}
		hs_free_database(filter_db);
		filter_db = 0;
		return 0;
	}

	if (!strcasecmp(data, "abort")) {
		if (state != FILTER_FILLING) {
			if (error) *error = "not filling";
			return -1;
		}
		state = filter_db ? FILTER_LOADED : FILTER_EMPTY;
		rb_free(filter_data);
		filter_data = 0;
		filter_data_len = 0;
		return 0;
	}

	if (strcmp(check, check_str) != 0) {
		if (error) *error = "check strings don't match";
		return -1;
	}

	if (!strcasecmp(data, "apply")) {
		if (state != FILTER_FILLING) {
			if (error) *error = "not loading anything";
			return -1;
		}
		hs_database_t *db;
		hs_error_t r = hs_deserialize_database(filter_data, filter_data_len, &db);
		if (r != HS_SUCCESS) {
			if (error) *error = "couldn't deserialize db";
			return -1;
		}
		r = hs_alloc_scratch(db, &filter_scratch);
		if (r != HS_SUCCESS) {
			if (error) *error = "couldn't allocate scratch";
			hs_free_database(db);
			return -1;
		}
		if (filter_db) {
			hs_free_database(filter_db);
		}
		state = FILTER_LOADED;
		filter_db = db;
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
			"New filters loaded.");
		rb_free(filter_data);
		filter_data = 0;
		filter_data_len = 0;
		return 0;
	}

	if (*data != '+') {
		if (error) *error = "unknown command or data doesn't start with +";
		return -1;
	}

	data += 1;

	if (state == FILTER_FILLING) {
		int dl;
		unsigned char *d = rb_base64_decode((unsigned char *)data, strlen(data), &dl);
		if (!d) {
			if (error) *error = "invalid data";
			return -1;
		}
		if (filter_data_len + dl > 10000000ul) {
			if (error) *error = "data over size limit";
			rb_free(d);
			return -1;
		}
		filter_data = rb_realloc(filter_data, filter_data_len + dl);
		memcpy(filter_data + filter_data_len, d, dl);
		rb_free(d);
		filter_data_len += dl;
	} else {
		if (error) *error = "send \"new\" first";
		return -1;
	}
	return 0;
}

/* /SETFILTER [server-mask] <check> { NEW | APPLY | <data> }
 * <check> must be the same for the entirety of a new...data...apply run,
 *   and exists just to ensure runs don't mix
 * NEW prepares a buffer to receive a hyperscan database
 * <data> is base64 encoded chunks of hyperscan database, which are decoded
 *   and appended to the buffer
 * APPLY deserialises the buffer and sets the resulting hyperscan database
 *   as the one to use for filtering */
static void
mo_setfilter(struct MsgBuf *msgbuf, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	int for_me = 0;
	const char *check;
	const char *data;
	if (!IsOperAdmin(source_p)) {
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "admin");
		return;
	}
	if (parc == 4) {
		check = parv[2];
		data = parv[3];
		if(match(parv[1], me.name)) {
			for_me = 1;
		}
		sendto_match_servs(source_p, parv[1],
			CAP_ENCAP, NOCAPS,
			"ENCAP %s SETFILTER %s :%s", parv[1], check, data);
	} else if (parc == 3) {
		check = parv[1];
		data = parv[2];
		for_me = 1;
	} else {
		sendto_one_notice(source_p, ":SETFILTER needs 2 or 3 params, have %d", parc - 1);
		return;
	}
	if (for_me) {
		const char *error;
		int r = setfilter(check, data, &error);
		if (r) {
			sendto_one_notice(source_p, ":SETFILTER failed: %s", error);
		} else {
			sendto_one_notice(source_p, ":SETFILTER ok");
		}
	}
}

static void
me_setfilter(struct MsgBuf *msgbuf, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	if(!IsPerson(source_p))
		return;

	const char *error;
	int r = setfilter(parv[1], parv[2], &error);
	if (r) {
		sendto_one_notice(source_p, ":SETFILTER failed: %s", error);
	}

	return;
}

/* will be called for every match
 * hyperscan provides us one piece of information about the expression
 * matched, an integer ID. we're co-opting the lowest 3 bits of this
 * as a flag set. conveniently, this means all we really need to do
 * here is or the IDs together. */
int match_callback(unsigned id,
                   unsigned long long from,
                   unsigned long long to,
                   unsigned flags,
                   void *context_)
{
	unsigned *context = context_;
	*context |= id;
	return 0;
}

static char check_buffer[2000];
static char clean_buffer[BUFSIZE];

unsigned match_message(const char *prefix,
                       struct Client *source,
                       const char *command,
                       const char *target,
                       const char *msg)
{
	unsigned state = 0;
	if (!filter_enable)
		return 0;
	if (!filter_db)
		return 0;
	if (!command)
		return 0;
	snprintf(check_buffer, sizeof check_buffer, "%s:%s!%s@%s#%c %s%s%s :%s",
	         prefix,
#if FILTER_NICK
	         source ? source->name : "*",
#else
	         "*",
#endif
#if FILTER_USER
	         source ? source->username : "*",
#else
	         "*",
#endif
#if FILTER_HOST
	         source ? source->host : "*",
#else
	         "*",
#endif
	         source && source->user && source->user->suser[0] != '\0' ? '1' : '0',
	         command,
	         target ? " " : "",
	         target ? target : "",
	         msg);
	hs_error_t r = hs_scan(filter_db, check_buffer, strlen(check_buffer), 0, filter_scratch, match_callback, &state);
	if (r != HS_SUCCESS && r != HS_SCAN_TERMINATED)
		return 0;
	return state;
}

void
filter_msg_user(void *data_)
{
	hook_data_privmsg_user *data = data_;
	struct Client *s = data->source_p;
	/* we only need to filter once */
	if (!MyClient(s)) {
		return;
	}
	/* opers are immune to checking, for obvious reasons
	 * anything sent to an oper is also immune, because that should make it
	 * less impossible to deal with reports. */
	if (IsOper(s) || IsOper(data->target_p)) {
		return;
	}
	if (data->target_p->umodes & filter_umode) {
		return;
	}
	char *text = strcpy(clean_buffer, data->text);
	strip_colour(text);
	strip_unprintable(text);
	unsigned r = match_message("0", s, cmdname[data->msgtype], "0", data->text) |
	             match_message("1", s, cmdname[data->msgtype], "0", text);
	if (r & ACT_DROP) {
		if (data->msgtype == MESSAGE_TYPE_PRIVMSG) {
			sendto_one_numeric(s, ERR_CANNOTSENDTOCHAN,
			                   form_str(ERR_CANNOTSENDTOCHAN),
			                   data->target_p->name);
		}
		data->approved = 1;
	}
	if (r & ACT_ALARM) {
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
			"FILTER: %s!%s@%s [%s]",
			s->name, s->username, s->host, s->sockhost);
	}
	if (r & ACT_KILL) {
		data->approved = 1;
		exit_client(NULL, s, s, FILTER_EXIT_MSG);
	}
}

void
filter_msg_channel(void *data_)
{
	hook_data_privmsg_channel *data = data_;
	struct Client *s = data->source_p;
	/* we only need to filter once */
	if (!MyClient(s)) {
		return;
	}
	/* just normal oper immunity for channels. i'd like to have a mode that
	 * disables the filter per-channel, but that's for the future */
	if (IsOper(s)) {
		return;
	}
	if (data->chptr->mode.mode & filter_chmode) {
		return;
	}
	char *text = strcpy(clean_buffer, data->text);
	strip_colour(text);
	strip_unprintable(text);
	unsigned r = match_message("0", s, cmdname[data->msgtype], data->chptr->chname, data->text) |
	             match_message("1", s, cmdname[data->msgtype], data->chptr->chname, text);
	if (r & ACT_DROP) {
		if (data->msgtype == MESSAGE_TYPE_PRIVMSG) {
			sendto_one_numeric(s, ERR_CANNOTSENDTOCHAN,
			                   form_str(ERR_CANNOTSENDTOCHAN),
			                   data->chptr->chname);
		}
		data->approved = 1;
	}
	if (r & ACT_ALARM) {
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
			"FILTER: %s!%s@%s [%s]",
			s->name, s->username, s->host, s->sockhost);
	}
	if (r & ACT_KILL) {
		data->approved = 1;
		exit_client(NULL, s, s, FILTER_EXIT_MSG);
	}
}

void
filter_client_quit(void *data_)
{
	hook_data_client_quit *data = data_;
	struct Client *s = data->client;
	if (IsOper(s)) {
		return;
	}
	char *text = strcpy(clean_buffer, data->orig_reason);
	strip_colour(text);
	strip_unprintable(text);
	unsigned r = match_message("0", s, "QUIT", NULL, data->orig_reason) |
	             match_message("1", s, "QUIT", NULL, text);
	if (r & ACT_DROP) {
		data->reason = NULL;
	}
	if (r & ACT_ALARM) {
		sendto_realops_snomask(SNO_GENERAL, L_ALL | L_NETWIDE,
			"FILTER: %s!%s@%s [%s]",
			s->name, s->username, s->host, s->sockhost);
	}
	/* No point in doing anything with ACT_KILL */
}

void
filter_client_nick_change(void *data_)
{
	hook_data_nick_approval *data = data_;
	struct Client *s = data->client;
	if (IsOper(s)) {
		return;
	}

	unsigned r = match_message("0", s, "NICK", NULL, data->nick);
	if (r & ACT_DROP) {
		data->approved = 0;
	}
	if (r & ACT_ALARM) {
		sendto_realops_snomask(SNO_GENERAL, L_ALL | L_NETWIDE,
			"FILTER: %s!%s@%s [%s]",
			s->name, s->username, s->host, s->sockhost);
	}
	if (r & ACT_KILL) {
		exit_client(NULL, s, s, FILTER_EXIT_MSG);
	}
}

void
filter_client_nick_set(void *data_)
{
	hook_data_nick_approval *data = data_;
	struct Client *s = data->client;
	struct sockaddr *addr = (void *)&s->localClient->ip;

	if(find_conf_by_address(
			NULL, NULL, NULL, addr, CONF_EXEMPTDLINE | 1, GET_SS_FAMILY(addr), NULL, NULL))
		return;

	unsigned r = match_message("0", NULL, "NICK", NULL, data->nick);
	if (r & ACT_DROP) {
		data->approved = 0;
	}
	if (r & ACT_ALARM) {
		sendto_realops_snomask(SNO_GENERAL, L_ALL | L_NETWIDE,
			"FILTER:REGISTER: %s@%s",
			s->id, s->sockhost);
	}
	if (r & ACT_KILL) {
		exit_client(NULL, s, s, FILTER_EXIT_MSG);
	}
}

void
on_client_exit(void *data_)
{
	/* If we see a netsplit, abort the current FILTER_FILLING attempt */
	hook_data_client_exit *data = data_;

	if (!IsServer(data->target)) return;

	if (state == FILTER_FILLING) {
		state = filter_db ? FILTER_LOADED : FILTER_EMPTY;
	}
}
