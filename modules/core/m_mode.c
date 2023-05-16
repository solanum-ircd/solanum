/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_mode.c: Sets a user or channel mode.
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
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "s_user.h"
#include "s_conf.h"
#include "s_serv.h"
#include "logger.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "packet.h"
#include "s_newconf.h"

static const char mode_desc[] =
	"Provides the MODE and MLOCK client and server commands, and TS6 server-to-server TMODE and BMASK commands";

static void m_mode(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void ms_mode(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void ms_tmode(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void ms_mlock(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void ms_bmask(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void ms_ebmask(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message mode_msgtab = {
	"MODE", 0, 0, 0, 0,
	{mg_unreg, {m_mode, 2}, {m_mode, 3}, {ms_mode, 3}, mg_ignore, {m_mode, 2}}
};
struct Message tmode_msgtab = {
	"TMODE", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, {ms_tmode, 4}, {ms_tmode, 4}, mg_ignore, mg_ignore}
};
struct Message mlock_msgtab = {
	"MLOCK", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, {ms_mlock, 3}, {ms_mlock, 3}, mg_ignore, mg_ignore}
};
struct Message bmask_msgtab = {
	"BMASK", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, {ms_bmask, 5}, mg_ignore, mg_ignore}
};
struct Message ebmask_msgtab = {
	"EBMASK", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, {ms_ebmask, 5}, mg_ignore, mg_ignore}
};

mapi_clist_av1 mode_clist[] = { &mode_msgtab, &tmode_msgtab, &mlock_msgtab, &bmask_msgtab, &ebmask_msgtab, NULL };

DECLARE_MODULE_AV2(mode, NULL, NULL, mode_clist, NULL, NULL, NULL, NULL, mode_desc);

/*
 * m_mode - MODE command handler
 * parv[1] - channel
 */
static void
m_mode(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr = NULL;
	struct membership *msptr;
	int n = 2;
	const char *dest;
	int operspy = 0;

	dest = parv[1];

	if(IsOperSpy(source_p) && *dest == '!')
	{
		dest++;
		operspy = 1;

		if(EmptyString(dest))
		{
			sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS),
				   me.name, source_p->name, "MODE");
			return;
		}
	}

	/* Now, try to find the channel in question */
	if(!IsChanPrefix(*dest))
	{
		/* if here, it has to be a non-channel name */
		user_mode(client_p, source_p, parc, parv);
		return;
	}

	if(!check_channel_name(dest))
	{
		sendto_one_numeric(source_p, ERR_BADCHANNAME, form_str(ERR_BADCHANNAME), parv[1]);
		return;
	}

	chptr = find_channel(dest);

	if(chptr == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return;
	}

	/* Now know the channel exists */
	if(parc < n + 1)
	{
		if(operspy)
			report_operspy(source_p, "MODE", chptr->chname);

		sendto_one(source_p, form_str(RPL_CHANNELMODEIS),
			   me.name, source_p->name, parv[1],
			   operspy ? channel_modes(chptr, &me) : channel_modes(chptr, source_p));

		sendto_one(source_p, form_str(RPL_CREATIONTIME),
			   me.name, source_p->name, parv[1], (long long)chptr->channelts);
	}
	else
	{
		msptr = find_channel_membership(chptr, source_p);

		set_channel_mode(client_p, source_p, chptr, msptr, parc - n, parv + n);
	}
}

static void
ms_mode(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr;

	chptr = find_channel(parv[1]);

	if(chptr == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return;
	}

	set_channel_mode(client_p, source_p, chptr, NULL, parc - 2, parv + 2);
}

static void
ms_tmode(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr = NULL;
	struct membership *msptr;

	/* Now, try to find the channel in question */
	if(!IsChanPrefix(parv[2][0]) || !check_channel_name(parv[2]))
	{
		sendto_one_numeric(source_p, ERR_BADCHANNAME, form_str(ERR_BADCHANNAME), parv[2]);
		return;
	}

	chptr = find_channel(parv[2]);

	if(chptr == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[2]);
		return;
	}

	/* TS is higher, drop it. */
	if(atol(parv[1]) > chptr->channelts)
		return;

	if(IsServer(source_p))
	{
		set_channel_mode(client_p, source_p, chptr, NULL, parc - 3, parv + 3);
	}
	else
	{
		msptr = find_channel_membership(chptr, source_p);

		set_channel_mode(client_p, source_p, chptr, msptr, parc - 3, parv + 3);
	}
}

static void
ms_mlock(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr = NULL;

	/* Now, try to find the channel in question */
	if(!IsChanPrefix(parv[2][0]) || !check_channel_name(parv[2]))
	{
		sendto_one_numeric(source_p, ERR_BADCHANNAME, form_str(ERR_BADCHANNAME), parv[2]);
		return;
	}

	chptr = find_channel(parv[2]);

	if(chptr == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
				   form_str(ERR_NOSUCHCHANNEL), parv[2]);
		return;
	}

	/* TS is higher, drop it. */
	if(atol(parv[1]) > chptr->channelts)
		return;

	if(IsServer(source_p))
		set_channel_mlock(client_p, source_p, chptr, parv[3], true);
}

static void
possibly_remove_lower_forward(struct Client *fakesource_p, int mems,
		struct Channel *chptr, rb_dlink_list *banlist, int mchar,
		const char *mask, const char *forward)
{
	struct Ban *actualBan;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, banlist->head)
	{
		actualBan = ptr->data;
		if(!irccmp(actualBan->banstr, mask) &&
				(actualBan->forward == NULL ||
				 irccmp(actualBan->forward, forward) < 0))
		{
			sendto_channel_local(fakesource_p, mems, chptr, ":%s MODE %s -%c %s%s%s",
					fakesource_p->name,
					chptr->chname,
					mchar,
					actualBan->banstr,
					actualBan->forward ? "$" : "",
					actualBan->forward ? actualBan->forward : "");
			rb_dlinkDelete(&actualBan->node, banlist);
			free_ban(actualBan);
			return;
		}
	}
}

static void
do_bmask(bool extended, struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static char output[BUFSIZE];
	static char parabuf[BUFSIZE];
	static char degrade[BUFSIZE];
	static char squitreason[120];
	struct Channel *chptr;
	struct Ban *banptr;
	rb_dlink_list *banlist;
	char *s, *mask, *forward, *who;
	char *output_ptr;
	char *param_ptr;
	char *degrade_ptr;
	long mode_type;
	int mlen;
	int plen = 0;
	int tlen;
	int arglen;
	int modecount = 0;
	int needcap = NOCAPS;
	int mems;
	time_t when = (long)rb_current_time();
	struct Client *fakesource_p;

	if(!IsChanPrefix(parv[2][0]) || !check_channel_name(parv[2]))
		return;

	if((chptr = find_channel(parv[2])) == NULL)
		return;

	/* TS is higher, drop it. */
	if(atol(parv[1]) > chptr->channelts)
		return;

	switch (parv[3][0])
	{
	case 'b':
		banlist = &chptr->banlist;
		mode_type = CHFL_BAN;
		mems = ALL_MEMBERS;
		break;

	case 'e':
		banlist = &chptr->exceptlist;
		mode_type = CHFL_EXCEPTION;
		needcap = CAP_EX;
		mems = ONLY_CHANOPS;
		break;

	case 'I':
		banlist = &chptr->invexlist;
		mode_type = CHFL_INVEX;
		needcap = CAP_IE;
		mems = ONLY_CHANOPS;
		break;

	case 'q':
		banlist = &chptr->quietlist;
		mode_type = CHFL_QUIET;
		mems = ALL_MEMBERS;
		break;

		/* maybe we should just blindly propagate this? */
	default:
		return;
	}

	parabuf[0] = '\0';
	s = LOCAL_COPY(parv[4]);

	/* Hide connecting server on netburst -- jilles */
	if (ConfigServerHide.flatten_links && !HasSentEob(source_p))
		fakesource_p = &me;
	else
		fakesource_p = source_p;
	who = fakesource_p->name;

	mlen = sprintf(output, ":%s MODE %s +", fakesource_p->name, chptr->chname);
	output_ptr = output + mlen;
	param_ptr = parabuf;
	degrade_ptr = degrade;

	while(*s == ' ')
		s++;

	s = strtok(s, " ");

	while(!EmptyString(s))
	{
		if(*s == ':')
		{
			/* ban with a leading ':' -- this will break the protocol */
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				"Link %s dropped, invalid BMASK mask (%s)", source_p->name, s);
			snprintf(squitreason, sizeof squitreason, "Invalid BMASK mask (%s)", s);
			exit_client(client_p, client_p, client_p, squitreason);
			return;
		}

		tlen = strlen(s);

		/* I dont even want to begin parsing this.. */
		if(tlen > MODEBUFLEN)
			break;

		if((forward = strchr(s+1, '$')) != NULL)
		{
			*forward++ = '\0';
			if(*forward == '\0')
				tlen--, forward = NULL;
			else
				possibly_remove_lower_forward(fakesource_p,
						mems, chptr, banlist,
						parv[3][0], s, forward);
		}

		mask = s;
		if (extended) {
			when = atol(strtok(NULL, " "));
			who = strtok(NULL, " ");
			if (who == NULL)
			{
				/* EBMASK params don't divide by 3, so we have an incomplete chunk */
				sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
					"Link %s dropped, invalid EBMASK chunk", source_p->name);
				snprintf(squitreason, sizeof squitreason, "Invalid EBMASK chunk");
				exit_client(client_p, client_p, client_p, squitreason);
				return;
			}

			arglen = sprintf(degrade_ptr, "%s ", mask);
			degrade_ptr += arglen;
		}

		if((banptr = add_id(fakesource_p, chptr, mask, forward, banlist, mode_type)) != NULL)
		{
			banptr->when = when;
			rb_free(banptr->who);
			banptr->who = rb_strdup(who);

			/* this new one wont fit.. */
			if(mlen + MAXMODEPARAMS + plen + tlen > BUFSIZE - 5 ||
			   modecount >= MAXMODEPARAMS)
			{
				*output_ptr = '\0';
				*(param_ptr - 1) = '\0';
				sendto_channel_local(fakesource_p, mems, chptr, "%s %s", output, parabuf);

				output_ptr = output + mlen;
				param_ptr = parabuf;
				plen = modecount = 0;
			}

			if (forward != NULL)
				forward[-1] = '$';

			*output_ptr++ = parv[3][0];
			arglen = sprintf(param_ptr, "%s ", mask);
			param_ptr += arglen;
			plen += arglen;
			modecount++;
		}

		s = strtok(NULL, " ");
	}

	if(modecount)
	{
		*output_ptr = '\0';
		*(param_ptr - 1) = '\0';
		sendto_channel_local(fakesource_p, mems, chptr, "%s %s", output, parabuf);
	}

	if (extended) {
		*(degrade_ptr - 1) = '\0';
		sendto_server(client_p, chptr, CAP_EBMASK | CAP_TS6 | needcap, NOCAPS, ":%s EBMASK %ld %s %s :%s",
			      source_p->id, (long) chptr->channelts, chptr->chname, parv[3], parv[4]);
		sendto_server(client_p, chptr, CAP_TS6 | needcap, CAP_EBMASK, ":%s BMASK %ld %s %s :%s",
			      source_p->id, (long) chptr->channelts, chptr->chname, parv[3], degrade);
	}
	else
		sendto_server(client_p, chptr, CAP_TS6 | needcap, NOCAPS, ":%s BMASK %ld %s %s :%s",
			      source_p->id, (long) chptr->channelts, chptr->chname, parv[3], parv[4]);
}

static void
ms_bmask(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	do_bmask(false, msgbuf_p, client_p, source_p, parc, parv);
}
static void
ms_ebmask(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	do_bmask(true, msgbuf_p, client_p, source_p, parc, parv);
}

