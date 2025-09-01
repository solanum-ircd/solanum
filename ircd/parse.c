/*
 *  Solanum: a slightly advanced ircd
 *  parse.c: The message parser.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *  Copyright (C) 2007-2016 Ariadne Conill <ariadne@dereferenced.org>
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
#include "parse.h"
#include "client.h"
#include "channel.h"
#include "hash.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "logger.h"
#include "s_stats.h"
#include "send.h"
#include "msg.h"
#include "msgbuf.h"
#include "s_conf.h"
#include "s_serv.h"
#include "packet.h"
#include "s_assert.h"

rb_dictionary *cmd_dict = NULL;
rb_dictionary *alias_dict = NULL;

const struct MsgBuf *incoming_message = NULL;
const struct Client *incoming_client = NULL;

static void cancel_clients(struct Client *, struct Client *);
static void remove_unknown(struct Client *, const char *, char *);

static void do_numeric(int, struct Client *, struct Client *, int, const char **);

static int handle_command(struct Message *, struct MsgBuf *, struct Client *, struct Client *);

static char buffer[1024];

/* turn a string into a parc/parv pair */

char *reconstruct_parv(int parc, const char *parv[])
{
	static char tmpbuf[BUFSIZE]; int i;

	rb_strlcpy(tmpbuf, parv[0], BUFSIZE);
	for (i = 1; i < parc; i++)
	{
		rb_strlcat(tmpbuf, " ", BUFSIZE);
		rb_strlcat(tmpbuf, parv[i], BUFSIZE);
	}
	return tmpbuf;
}

/* parse()
 *
 * given a raw buffer, parses it and generates parv and parc
 */
void
parse(struct Client *client_p, char *pbuffer, char *bufend)
{
	struct Client *from = client_p;
	char *end;
	int res;
	int numeric = 0;
	struct Message *mptr;
	struct MsgBuf msgbuf;

	s_assert(MyConnect(client_p) &&
			(client_p->localClient->F != NULL ||
				client_p->localClient->localflags & LFLAGS_FAKE));
	if(IsAnyDead(client_p))
		return;

	end = bufend - 1;

	/* XXX this should be done before parse() is called */
	if(*end == '\n')
		*end-- = '\0';
	if(*end == '\r')
		*end = '\0';

	res = msgbuf_parse(&msgbuf, pbuffer);
	if (res)
	{
		if (IsClient(client_p) && (res == PARSE_UNTERMINATED_ORIGIN || res == PARSE_UNTERMINATED_TAGS))
			sendto_one(from, form_str(ERR_INPUTTOOLONG), me.name, from->name);

		ServerStats.is_empt++;
		return;
	}

	if (msgbuf.origin != NULL && IsServer(client_p))
	{
		from = find_client(msgbuf.origin);

		/* didnt find any matching client, issue a kill */
		if(from == NULL)
		{
			ServerStats.is_unpf++;
			remove_unknown(client_p, msgbuf.origin, pbuffer);
			return;
		}

		/* fake direction, hmm. */
		if(from->from != client_p)
		{
			ServerStats.is_wrdi++;
			cancel_clients(client_p, from);
			return;
		}
	}

	/* too many tags or tag data too long? */
	if (IsClient(client_p) && (msgbuf.n_tags == MAXTAGS || msgbuf.tagslen > TAGSPARTLEN + 2))
	{
		sendto_one(from, form_str(ERR_INPUTTOOLONG), me.name, from->name);
		return;
	}

	if(IsDigit(*msgbuf.cmd) && IsDigit(*(msgbuf.cmd + 1)) && IsDigit(*(msgbuf.cmd + 2)))
	{
		mptr = NULL;
		numeric = atoi(msgbuf.cmd);
		ServerStats.is_num++;
	}
	else
	{
		mptr = rb_dictionary_retrieve(cmd_dict, msgbuf.cmd);

		/* no command or its encap only, error */
		if(!mptr || !mptr->cmd)
		{
			if(IsPerson(from))
			{
				sendto_one(from, form_str(ERR_UNKNOWNCOMMAND),
					   me.name, from->name, msgbuf.cmd);
			}
			ServerStats.is_unco++;
			return;
		}
	}

	if(mptr == NULL)
	{
		do_numeric(numeric, client_p, from, msgbuf.n_para, msgbuf.para);
		return;
	}

	/* The tags array may be mutated via hooks; this holds the updated array */
	struct MsgBuf updated_msg;
	int ntags = msgbuf.n_tags;
	hook_data_message_tag tagdata;

	/* Filter out tags that we don't allow or recognize */
	memcpy(&updated_msg, &msgbuf, sizeof(struct MsgBuf));
	updated_msg.n_tags = 0;
	tagdata.source = from;
	tagdata.client = client_p;
	tagdata.message = &msgbuf;

	for (int i = ntags - 1; i >= 0; i--)
	{
		/* a hook can adjust key/value/capmask according to its needs;
		 * value can be set to NULL in order to strip a value
		 */
		tagdata.key = msgbuf.tags[i].key;
		tagdata.value = msgbuf.tags[i].value;
		tagdata.capmask = CLICAP_MESSAGE_TAGS;
		tagdata.approved = MESSAGE_TAG_REMOVE;

		if (tagdata.key == NULL)
			continue;

		size_t keylen = strlen(tagdata.key);
		if (keylen == 0 || (keylen == 1 && *tagdata.key == '+'))
			continue;

		/* If a tag is specified more than once, discard all but the final occurrence per spec;
		 * we iterate backwards so that we process later tags before earlier ones for this reason */
		if (msgbuf_get_tag(&updated_msg, tagdata.key) != NULL)
			continue;

		call_hook(h_message_tag, &tagdata);

		switch (tagdata.approved)
		{
		case MESSAGE_TAG_ALLOW:
			msgbuf_append_tag(&updated_msg, tagdata.key, tagdata.value, tagdata.capmask);
			break;

		case MESSAGE_TAG_REMOVE:
			/* tag is rejected, don't add it to the outbound list of tags */
			break;

		case MESSAGE_TAG_DROP:
			/* entire message is rejected, don't send at all */
			return;
		}
	}

	incoming_message = &updated_msg;
	incoming_client = client_p;

	if(handle_command(mptr, &updated_msg, client_p, from) < -1)
	{
		char *p;
		for (p = pbuffer; p <= end; p += 8)
		{
			/* HACK HACK */
			/* Its expected this nasty code can be removed
			 * or rewritten later if still needed.
			 */
			if((p + 8) > end)
			{
				for (; p <= end; p++)
				{
					ilog(L_MAIN, "%02x |%c", p[0], p[0]);
				}
			}
			else
				ilog(L_MAIN,
				     "%02x %02x %02x %02x %02x %02x %02x %02x |%c%c%c%c%c%c%c%c",
				     p[0], p[1], p[2], p[3], p[4], p[5],
				     p[6], p[7], p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
		}
	}

	incoming_message = NULL;
	incoming_client = NULL;
}

/*
 * handle_command
 *
 * inputs	- pointer to message block
 * 		- pointer to message buffer
 *		- pointer to client
 *		- pointer to client message is from
 * output	- -1 if error from server
 * side effects	-
 */
static int
handle_command(struct Message *mptr, struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *from)
{
	struct MessageEntry ehandler;
	MessageHandler handler = 0;
	char squitreason[80];

	if(IsAnyDead(client_p))
		return -1;

	if(IsServer(client_p))
		mptr->rcount++;

	mptr->count++;

	ehandler = mptr->handlers[from->handler];
	handler = ehandler.handler;

	/* check right amount of params is passed... --is */
	if(msgbuf_p->n_para < ehandler.min_para ||
	   (ehandler.min_para && EmptyString(msgbuf_p->para[ehandler.min_para - 1])))
	{
		if(!IsServer(client_p))
		{
			sendto_one(client_p, form_str(ERR_NEEDMOREPARAMS),
				   me.name,
				   EmptyString(client_p->name) ? "*" : client_p->name,
				   mptr->cmd);
			if(MyClient(client_p))
				return (1);
			else
				return (-1);
		}

		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				     "Dropping server %s due to (invalid) command '%s'"
				     " with only %zu arguments (expecting %zu).",
				     client_p->name, mptr->cmd, msgbuf_p->n_para, ehandler.min_para);
		ilog(L_SERVER,
		     "Insufficient parameters (%zu < %zu) for command '%s' from %s.",
		     msgbuf_p->n_para, ehandler.min_para, mptr->cmd, client_p->name);
		snprintf(squitreason, sizeof squitreason,
				"Insufficient parameters (%zu < %zu) for command '%s'",
				msgbuf_p->n_para, ehandler.min_para, mptr->cmd);
		exit_client(client_p, client_p, client_p, squitreason);
		return (-1);
	}

	(*handler) (msgbuf_p, client_p, from, msgbuf_p->n_para, msgbuf_p->para);
	return (1);
}

void
handle_encap(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p,
	     const char *command, int parc, const char *parv[])
{
	struct Message *mptr;
	struct MessageEntry ehandler;
	MessageHandler handler = 0;

	mptr = rb_dictionary_retrieve(cmd_dict, command);

	if(mptr == NULL || mptr->cmd == NULL)
		return;

	ehandler = mptr->handlers[ENCAP_HANDLER];
	handler = ehandler.handler;

	if((size_t)parc < ehandler.min_para ||
	   (ehandler.min_para && EmptyString(parv[ehandler.min_para - 1])))
		return;

	(*handler) (msgbuf_p, client_p, source_p, parc, parv);
}

/*
 * clear_hash_parse()
 *
 * inputs       -
 * output       - NONE
 * side effects - MUST MUST be called at startup ONCE before
 *                any other keyword hash routine is used.
 */
void
clear_hash_parse()
{
	cmd_dict = rb_dictionary_create("command", rb_strcasecmp);
}

/* mod_add_cmd
 *
 * inputs	- command name
 *		- pointer to struct Message
 * output	- none
 * side effects - load this one command name
 *		  msg->count msg->bytes is modified in place, in
 *		  modules address space. Might not want to do that...
 */
void
mod_add_cmd(struct Message *msg)
{
	s_assert(msg != NULL);
	if(msg == NULL)
		return;

	if (rb_dictionary_find(cmd_dict, msg->cmd) != NULL) {
		ilog(L_MAIN, "Add command: %s already exists", msg->cmd);
		s_assert(0);
		return;
	}

	msg->count = 0;
	msg->rcount = 0;
	msg->bytes = 0;

	rb_dictionary_add(cmd_dict, msg->cmd, msg);
}

/* mod_del_cmd
 *
 * inputs	- command name
 * output	- none
 * side effects - unload this one command name
 */
void
mod_del_cmd(struct Message *msg)
{
	s_assert(msg != NULL);
	if(msg == NULL)
		return;

	if (rb_dictionary_delete(cmd_dict, msg->cmd) == NULL) {
		ilog(L_MAIN, "Delete command: %s not found", msg->cmd);
		s_assert(0);
	}
}

/* cancel_clients()
 *
 * inputs	- client who sent us the message, client with fake
 *		  direction
 * outputs	- a given warning about the fake direction
 * side effects -
 */
static void
cancel_clients(struct Client *client_p, struct Client *source_p)
{
	/* ok, fake prefix happens naturally during a burst on a nick
	 * collision with TS5, we cant kill them because one client has to
	 * survive, so we just send an error.
	 */
	if(IsServer(source_p) || IsMe(source_p))
	{
		sendto_realops_snomask(SNO_DEBUG, L_ALL,
				     "Message for %s[%s] from %s",
				     source_p->name, source_p->from->name,
				     client_p->name);
	}
	else
	{
		sendto_realops_snomask(SNO_DEBUG, L_ALL,
				     "Message for %s[%s@%s!%s] from %s (TS, ignored)",
				     source_p->name,
				     source_p->username,
				     source_p->host,
				     source_p->from->name,
				     client_p->name);
	}
}

/* remove_unknown()
 *
 * inputs	- client who gave us message, supposed sender, buffer
 * output	-
 * side effects	- kills issued for clients, squits for servers
 */
static void
remove_unknown(struct Client *client_p, const char *lsender, char *lbuffer)
{
	int slen = strlen(lsender);
	char sid[4];
	struct Client *server;

	/* meepfoo	is a nickname (ignore)
	 * #XXXXXXXX	is a UID (KILL)
	 * #XX		is a SID (SQUIT)
	 * meep.foo	is a server (SQUIT)
	 */
	if((IsDigit(lsender[0]) && slen == 3) ||
	   (strchr(lsender, '.') != NULL))
	{
		sendto_realops_snomask(SNO_DEBUG, L_NETWIDE,
				     "Unknown prefix (%s) from %s, Squitting %s",
				     lbuffer, client_p->name, lsender);

		sendto_one(client_p,
			   ":%s SQUIT %s :(Unknown prefix (%s) from %s)",
			   get_id(&me, client_p), lsender,
			   lbuffer, client_p->name);
	}
	else if(!IsDigit(lsender[0]))
		;
	else if(slen != 9)
		sendto_realops_snomask(SNO_DEBUG, L_NETWIDE,
				     "Invalid prefix (%s) from %s",
				     lbuffer, client_p->name);
	else
	{
		memcpy(sid, lsender, 3);
		sid[3] = '\0';
		server = find_server(NULL, sid);
		if (server != NULL && server->from == client_p)
			sendto_one(client_p, ":%s KILL %s :%s (Unknown Client)",
					get_id(&me, client_p), lsender, me.name);
	}
}



/*
 *
 *      parc    number of arguments ('sender' counted as one!)
 *      parv[1]..parv[parc-1]
 *              pointers to additional parameters, this is a NULL
 *              terminated list (parv[parc] == NULL).
 *
 * *WARNING*
 *      Numerics are mostly error reports. If there is something
 *      wrong with the message, just *DROP* it! Don't even think of
 *      sending back a neat error message -- big danger of creating
 *      a ping pong error message...
 */
static void
do_numeric(int numeric, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	struct Channel *chptr;

	if(parc < 2 || !IsServer(source_p))
		return;

	/* Remap low number numerics. */
	if(numeric < 100)
		numeric += 100;

	/*
	 * Prepare the parameter portion of the message into 'buffer'.
	 * (Because the buffer is twice as large as the message buffer
	 * for the socket, no overflow can occur here... ...on current
	 * assumptions--bets are off, if these are changed --msa)
	 * Note: if buffer is non-empty, it will begin with SPACE.
	 */
	if(parc > 1)
	{
		char *t = buffer;	/* Current position within the buffer */
		int i;
		int tl;		/* current length of presently being built string in t */
		for (i = 2; i < (parc - 1); i++)
		{
			tl = sprintf(t, " %s", parv[i]);
			t += tl;
		}
		sprintf(t, " :%s", parv[parc - 1]);
	}

	if((target_p = find_client(parv[1])) != NULL)
	{
		if(IsMe(target_p))
		{
			/*
			 * We shouldn't get numerics sent to us,
			 * any numerics we do get indicate a bug somewhere..
			 */
			/* ugh.  this is here because of nick collisions.  when two servers
			 * relink, they burst each other their nicks, then perform collides.
			 * if there is a nick collision, BOTH servers will kill their own
			 * nicks, and BOTH will kill the other servers nick, which wont exist,
			 * because it will have been already killed by the local server.
			 *
			 * unfortunately, as we cant guarantee other servers will do the
			 * "right thing" on a nick collision, we have to keep both kills.
			 * ergo we need to ignore ERR_NOSUCHNICK. --fl_
			 */
			/* quick comment. This _was_ tried. i.e. assume the other servers
			 * will do the "right thing" and kill a nick that is colliding.
			 * unfortunately, it did not work. --Dianora
			 */
			/* note, now we send PING on server connect, we can
			 * also get ERR_NOSUCHSERVER..
			 */
			if(numeric != ERR_NOSUCHNICK &&
			   numeric != ERR_NOSUCHSERVER)
				sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
						     "*** %s(via %s) sent a %03d numeric to me: %s",
						     source_p->name,
						     client_p->name, numeric, buffer);
			return;
		}
		else if(target_p->from == client_p)
		{
			/* This message changed direction (nick collision?)
			 * ignore it.
			 */
			return;
		}

		/* csircd will send out unknown umode flag for +a (admin), drop it here. */
		if(numeric == ERR_UMODEUNKNOWNFLAG && MyClient(target_p))
			return;

		/* Fake it for server hiding, if its our client */
		sendto_one(target_p, ":%s %03d %s%s",
			   get_id(source_p, target_p), numeric,
			   get_id(target_p, target_p), buffer);
		return;
	}
	else if((chptr = find_channel(parv[1])) != NULL)
		sendto_channel_flags(client_p, ALL_MEMBERS, source_p, chptr,
				     "%03d %s%s",
				     numeric, chptr->chname, buffer);
}

void
m_not_oper(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	sendto_one_numeric(source_p, ERR_NOPRIVILEGES, form_str(ERR_NOPRIVILEGES));
}

void
m_unregistered(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(IsAnyServer(client_p))
		return;

	/* bit of a hack.
	 * I don't =really= want to waste a bit in a flag
	 * number_of_nick_changes is only really valid after the client
	 * is fully registered..
	 */
	if(client_p->localClient->number_of_nick_changes == 0)
	{
		sendto_one(client_p, form_str(ERR_NOTREGISTERED), me.name);
		client_p->localClient->number_of_nick_changes++;
	}
}

void
m_registered(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	sendto_one(client_p, form_str(ERR_ALREADYREGISTRED), me.name, source_p->name);
}

void
m_ignore(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	/* Does nothing */
}
