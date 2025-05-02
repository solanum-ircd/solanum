/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_join.c: Joins a channel.
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
#include "send.h"
#include "s_serv.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "packet.h"
#include "chmode.h"
#include "ratelimit.h"
#include "s_assert.h"
#include "hook.h"

static const char join_desc[] = "Provides the JOIN and TS6 SJOIN commands to facilitate joining and creating channels";

static void m_join(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void ms_join(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void ms_sjoin(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

static int h_can_create_channel;
static int h_channel_join;
static int h_channel_lowerts;

struct Message join_msgtab = {
	"JOIN", 0, 0, 0, 0,
	{mg_unreg, {m_join, 2}, {ms_join, 2}, mg_ignore, mg_ignore, {m_join, 2}}
};

struct Message sjoin_msgtab = {
	"SJOIN", 0, 0, 0, 0,
	{mg_unreg, mg_ignore, mg_ignore, {ms_sjoin, 4}, mg_ignore, mg_ignore}
};

mapi_clist_av1 join_clist[] = { &join_msgtab, &sjoin_msgtab, NULL };

mapi_hlist_av1 join_hlist[] = {
	{ "can_create_channel", &h_can_create_channel },
	{ "channel_join", &h_channel_join },
	{ "channel_lowerts", &h_channel_lowerts },
	{ NULL, NULL },
};

DECLARE_MODULE_AV2(join, NULL, NULL, join_clist, join_hlist, NULL, NULL, NULL, join_desc);

static void do_join_0(struct Client *client_p, struct Client *source_p);
static bool check_channel_name_loc(struct Client *source_p, const char *name);
static void send_join_error(struct Client *source_p, int numeric, const char *name);

static char *set_final_mode(char *mbuf, char *parabuf, struct Mode *mode, struct Mode *oldmode);
static void remove_our_modes(struct Channel *chptr, struct Client *source_p);

static void remove_ban_list(struct Channel *chptr, struct Client *source_p,
			    rb_dlink_list * list, char c, int mems);

/* Check what we will forward to, without sending any notices to the user
 * -- jilles
 */
static struct Channel *
check_forward(struct Client *source_p, struct Channel *chptr,
	     char *key, int *err)
{
	int depth = 0, i;
	const char *next = NULL;

	/* The caller (m_join) is only interested in the reason
	 * for the original channel.
	 */
	if ((*err = can_join(source_p, chptr, key, &next)) == 0)
		return chptr;

	/* User is +Q, or forwarding disabled */
	if (IsNoForward(source_p) || !ConfigChannel.use_forward)
		return NULL;

	while (depth < 16)
	{
		if (next == NULL)
			return NULL;

		chptr = find_channel(next);
		/* Can only forward to existing channels */
		if (chptr == NULL)
			return NULL;
		/* Already on there... but don't send the original reason for
		 * being unable to join. It isn't their fault they're already
		 * on the channel, and it looks hostile otherwise.
		 * --Elizafox
		 */
		if (IsMember(source_p, chptr))
		{
			*err = ERR_USERONCHANNEL; /* I'm borrowing this for now. --Elizafox */
			return NULL;
		}
		/* Juped. Sending a warning notice would be unfair */
		if (hash_find_resv(chptr->chname))
			return NULL;
		/* Don't forward to +Q channel */
		if (chptr->mode.mode & MODE_DISFORWARD)
			return NULL;

		i = can_join(source_p, chptr, key, &next);
		if (i == 0)
			return chptr;
		depth++;
	}

	return NULL;
}

/*
 * m_join
 *      parv[1] = channel
 *      parv[2] = channel password (key)
 */
static void
m_join(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static char jbuf[BUFSIZE];
	struct Channel *chptr = NULL, *chptr2 = NULL;
	struct ConfItem *aconf;
	char *name;
	char *key = NULL;
	const char *modes;
	int i, flags = 0;
	char *p = NULL, *p2 = NULL;
	char *chanlist;
	char *mykey;

	jbuf[0] = '\0';

	/* rebuild the list of channels theyre supposed to be joining.
	 * this code has a side effect of losing keys, but..
	 */
	chanlist = LOCAL_COPY(parv[1]);
	for(name = rb_strtok_r(chanlist, ",", &p); name; name = rb_strtok_r(NULL, ",", &p))
	{
		/* check the length and name of channel is ok */
		if(!check_channel_name_loc(source_p, name) || (strlen(name) > LOC_CHANNELLEN))
		{
			sendto_one_numeric(source_p, ERR_BADCHANNAME,
					   form_str(ERR_BADCHANNAME), (unsigned char *) name);
			continue;
		}

		/* join 0 parts all channels */
		if(*name == '0' && (name[1] == ',' || name[1] == '\0') && name == chanlist)
		{
			rb_strlcpy(jbuf, "0", sizeof(jbuf));
			continue;
		}

		/* check it begins with a valid channel prefix per policy. */
		else if (!IsChannelName(name))
		{
			sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL,
					   form_str(ERR_NOSUCHCHANNEL), name);
			continue;
		}

		/* see if its resv'd */
		if(!IsExemptResv(source_p) && (aconf = hash_find_resv(name)))
		{
			sendto_one_numeric(source_p, ERR_BADCHANNAME,
					   form_str(ERR_BADCHANNAME), name);

			/* dont warn for opers */
			if(!IsExemptJupe(source_p) && !IsOper(source_p))
				sendto_realops_snomask(SNO_SPY, L_NETWIDE,
						     "User %s (%s@%s) is attempting to join locally juped channel %s (%s)",
						     source_p->name, source_p->username,
						     source_p->orighost, name, aconf->passwd);
			/* dont update tracking for jupe exempt users, these
			 * are likely to be spamtrap leaves
			 */
			else if(IsExemptJupe(source_p))
				aconf->port--;

			continue;
		}

		if(splitmode && !IsOperGeneral(source_p) && (*name != '&') &&
		   ConfigChannel.no_join_on_split)
		{
			sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
				   me.name, source_p->name, name);
			continue;
		}

		if(*jbuf)
			(void) strcat(jbuf, ",");
		(void) rb_strlcat(jbuf, name, sizeof(jbuf));
	}

	if(parc > 2)
	{
		mykey = LOCAL_COPY(parv[2]);
		key = rb_strtok_r(mykey, ",", &p2);
	}

	for(name = rb_strtok_r(jbuf, ",", &p); name;
	    key = (key) ? rb_strtok_r(NULL, ",", &p2) : NULL, name = rb_strtok_r(NULL, ",", &p))
	{
		hook_data_channel_activity hook_info;

		/* JOIN 0 simply parts all channels the user is in */
		if(*name == '0' && !atoi(name))
		{
			if(source_p->user->channel.head == NULL)
				continue;

			do_join_0(&me, source_p);
			continue;
		}

		/* look for the channel */
		if((chptr = find_channel(name)) != NULL)
		{
			if(IsMember(source_p, chptr))
				continue;

			flags = 0;
		}
		else
		{
			hook_data_can_create_channel moduledata;

			moduledata.client = source_p;
			moduledata.name = name;
			moduledata.approved = 0;

			call_hook(h_can_create_channel, &moduledata);

			if(moduledata.approved != 0)
			{
				if(moduledata.approved != ERR_CUSTOM)
					send_join_error(source_p,
							moduledata.approved,
							name);
				continue;
			}

			if(splitmode && !IsOperGeneral(source_p) && (*name != '&') &&
			   ConfigChannel.no_create_on_split)
			{
				sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
					   me.name, source_p->name, name);
				continue;
			}

			flags = CHFL_CHANOP;
		}

		if((rb_dlink_list_length(&source_p->user->channel) >=
		    (unsigned long) ConfigChannel.max_chans_per_user) &&
		   (!IsExtendChans(source_p) ||
		    (rb_dlink_list_length(&source_p->user->channel) >=
		     (unsigned long) ConfigChannel.max_chans_per_user_large)))
		{
			sendto_one(source_p, form_str(ERR_TOOMANYCHANNELS),
				   me.name, source_p->name, name);
			continue;
		}

		if(chptr == NULL)	/* If I already have a chptr, no point doing this */
		{
			chptr = get_or_create_channel(source_p, name, NULL);

			if(chptr == NULL)
			{
				sendto_one(source_p, form_str(ERR_UNAVAILRESOURCE),
					   me.name, source_p->name, name);
				continue;
			}
		}

		/* If check_forward returns NULL, they couldn't join and there wasn't a usable forward channel. */
		if((chptr2 = check_forward(source_p, chptr, key, &i)) == NULL)
		{
			/* might be wrong, but is there any other better location for such?
			 * see extensions/chm_operonly.c for other comments on this
			 * -- dwr
			 */
			if(i != ERR_CUSTOM)
				send_join_error(source_p, i, name);
			continue;
		}
		else if(chptr != chptr2)
			sendto_one_numeric(source_p, ERR_LINKCHANNEL, form_str(ERR_LINKCHANNEL), name, chptr2->chname);

		chptr = chptr2;

		if(flags == 0 &&
				!IsOperGeneral(source_p) && !IsExemptSpambot(source_p))
			check_spambot_warning(source_p, name);

		/* add the user to the channel */
		add_user_to_channel(chptr, source_p, flags);
		if (chptr->mode.join_num &&
			rb_current_time() - chptr->join_delta >= chptr->mode.join_time)
		{
			chptr->join_count = 0;
			chptr->join_delta = rb_current_time();
		}
		chptr->join_count++;

		/* credit user for join */
		credit_client_join(source_p);

		/* we send the user their join here, because we could have to
		 * send a mode out next.
		 */
		send_channel_join(chptr, source_p);

		/* its a new channel, set +nt and burst. */
		if(flags & CHFL_CHANOP)
		{
			chptr->channelts = rb_current_time();
			chptr->mode.mode |= ConfigChannel.autochanmodes;
			modes = channel_modes(chptr, &me);

			sendto_channel_local(&me, ONLY_CHANOPS, chptr, ":%s MODE %s %s",
					     me.name, chptr->chname, modes);

			sendto_server(client_p, chptr, CAP_TS6, NOCAPS,
				      ":%s SJOIN %ld %s %s :@%s",
				      me.id, (long) chptr->channelts,
				      chptr->chname, modes, source_p->id);
		}
		else
		{
			sendto_server(client_p, chptr, CAP_TS6, NOCAPS,
				      ":%s JOIN %ld %s +",
				      use_id(source_p), (long) chptr->channelts,
				      chptr->chname);
		}

		del_invite(chptr, source_p);

		if(chptr->topic != NULL)
		{
			sendto_one(source_p, form_str(RPL_TOPIC), me.name,
				   source_p->name, chptr->chname, chptr->topic);

			sendto_one(source_p, form_str(RPL_TOPICWHOTIME),
				   me.name, source_p->name, chptr->chname,
				   chptr->topic_info,
				   (long long)chptr->topic_time);
		}

		channel_member_names(chptr, source_p, 1);

		hook_info.client = source_p;
		hook_info.chptr = chptr;
		hook_info.key = key;
		call_hook(h_channel_join, &hook_info);
	}
}

/*
 * ms_join
 *      parv[1] = channel TS
 *      parv[2] = channel
 *      parv[3] = "+", formerly channel modes but now unused
 * alternatively, a single "0" parameter parts all channels
 */
static void
ms_join(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static char modebuf[MODEBUFLEN];
	static char parabuf[MODEBUFLEN];
	struct Channel *chptr;
	static struct Mode mode;
	time_t oldts;
	time_t newts;
	bool isnew;
	bool keep_our_modes = true;
	rb_dlink_node *ptr, *next_ptr;
	char *mbuf;

	/* special case for join 0 */
	if((parv[1][0] == '0') && (parv[1][1] == '\0') && parc == 2)
	{
		do_join_0(client_p, source_p);
		return;
	}

	if(parc < 4)
		return;

	if(!IsChannelName(parv[2]) || !check_channel_name(parv[2]))
		return;

	/* joins for local channels cant happen. */
	if(parv[2][0] == '&')
		return;

	mbuf = modebuf;
	mode.key[0] = mode.forward[0] = '\0';
	mode.mode = mode.limit = mode.join_num = mode.join_time = 0;

	if((chptr = get_or_create_channel(source_p, parv[2], &isnew)) == NULL)
		return;

	newts = atol(parv[1]);
	oldts = chptr->channelts;

	/* making a channel TS0 */
	if(!isnew && !newts && oldts)
	{
		sendto_channel_local(&me, ALL_MEMBERS, chptr,
				     ":%s NOTICE %s :*** Notice -- TS for %s changed from %ld to 0",
				     me.name, chptr->chname, chptr->chname, (long) oldts);
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Server %s changing TS on %s from %ld to 0",
				     source_p->name, chptr->chname, (long) oldts);
	}

	if(isnew)
		chptr->channelts = newts;
	else if(newts == 0 || oldts == 0)
		chptr->channelts = 0;
	else if(newts == oldts)
		;
	else if(newts < oldts)
	{
		keep_our_modes = false;
		chptr->channelts = newts;
	}

	/* Lost the TS, other side wins, so remove modes on this side */
	if(!keep_our_modes)
	{
		mbuf = set_final_mode(mbuf, parabuf, &mode, &chptr->mode);
		chptr->mode = mode;
		remove_our_modes(chptr, source_p);
		RB_DLINK_FOREACH_SAFE(ptr, next_ptr, chptr->invites.head)
		{
			del_invite(chptr, ptr->data);
		}
		/* If setting -j, clear join throttle state -- jilles */
		chptr->join_count = chptr->join_delta = 0;
		sendto_channel_local(&me, ALL_MEMBERS, chptr,
				     ":%s NOTICE %s :*** Notice -- TS for %s changed from %ld to %ld",
				     me.name, chptr->chname, chptr->chname,
				     (long) oldts, (long) newts);
		/* Update capitalization in channel name, this makes the
		 * capitalization timestamped like modes are -- jilles */
		strcpy(chptr->chname, parv[2]);
		if(*modebuf != '\0')
			sendto_channel_local(source_p->servptr, ALL_MEMBERS, chptr,
					     ":%s MODE %s %s %s",
					     source_p->servptr->name,
					     chptr->chname, modebuf, parabuf);
		*modebuf = *parabuf = '\0';

		/* since we're dropping our modes, we want to clear the mlock as well. --nenolod */
		set_channel_mlock(client_p, source_p, chptr, NULL, false);

		hook_data_channel hookdata;
		hookdata.client = source_p;
		hookdata.chptr = chptr;

		call_hook(h_channel_lowerts, &hookdata);
	}

	if(!IsMember(source_p, chptr))
	{
		add_user_to_channel(chptr, source_p, CHFL_PEON);
		if (chptr->mode.join_num &&
			rb_current_time() - chptr->join_delta >= chptr->mode.join_time)
		{
			chptr->join_count = 0;
			chptr->join_delta = rb_current_time();
		}
		chptr->join_count++;
		send_channel_join(chptr, source_p);
	}

	sendto_server(client_p, chptr, CAP_TS6, NOCAPS,
		      ":%s JOIN %ld %s +",
		      source_p->id, (long) chptr->channelts, chptr->chname);
}

static void
ms_sjoin(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static char modebuf[MODEBUFLEN];
	static char parabuf[MODEBUFLEN];
	static char buf_uid[BUFSIZE];
	static const char empty_modes[] = "0";
	struct Channel *chptr;
	struct Client *target_p, *fakesource_p;
	time_t newts;
	time_t oldts;
	static struct Mode mode, *oldmode;
	const char *modes;
	int args = 0;
	bool keep_our_modes = true;
	bool keep_new_modes = true;
	int fl;
	bool isnew;
	int mlen_uid;
	int len_uid;
	int len;
	int joins = 0;
	const char *s;
	char *ptr_uid;
	char *p;
	int i, joinc = 0, timeslice = 0;
	static char empty[] = "";
	rb_dlink_node *ptr, *next_ptr;
	char *mbuf;
	int pargs;
	const char *para[MAXMODEPARAMS];

	if(parc < 5)
		return;

	if(!IsChannelName(parv[2]) || !check_channel_name(parv[2]))
		return;

	/* SJOIN's for local channels can't happen. */
	if(*parv[2] == '&')
		return;

	modebuf[0] = parabuf[0] = mode.key[0] = mode.forward[0] = '\0';
	pargs = mode.mode = mode.limit = mode.join_num = mode.join_time = 0;

	/* Hide connecting server on netburst -- jilles */
	if (ConfigServerHide.flatten_links && !HasSentEob(source_p))
		fakesource_p = &me;
	else
		fakesource_p = source_p;

	mbuf = modebuf;
	newts = atol(parv[1]);

	s = parv[3];
	while (*s)
	{
		switch (*(s++))
		{
		case 'f':
			rb_strlcpy(mode.forward, parv[4 + args], sizeof(mode.forward));
			args++;
			if(parc < 5 + args)
				return;
			break;
		case 'j':
			sscanf(parv[4 + args], "%d:%d", &joinc, &timeslice);
			args++;
			mode.join_num = joinc;
			mode.join_time = timeslice;
			if(parc < 5 + args)
				return;
			break;
		case 'k':
			rb_strlcpy(mode.key, parv[4 + args], sizeof(mode.key));
			args++;
			if(parc < 5 + args)
				return;
			break;
		case 'l':
			mode.limit = atoi(parv[4 + args]);
			args++;
			if(parc < 5 + args)
				return;
			break;
		default:
			if(chmode_flags[(int) *s] != 0)
			{
				mode.mode |= chmode_flags[(int) *s];
			}
		}
	}

	if(parv[args + 4])
	{
		s = parv[args + 4];

		/* remove any leading spaces */
		while (*s == ' ')
			s++;
	}
	else
		s = "";

	if((chptr = get_or_create_channel(source_p, parv[2], &isnew)) == NULL)
		return;	/* channel name too long? */


	oldts = chptr->channelts;
	oldmode = &chptr->mode;

	if(!isnew && !newts && oldts)
	{
		sendto_channel_local(&me, ALL_MEMBERS, chptr,
				     ":%s NOTICE %s :*** Notice -- TS for %s "
				     "changed from %ld to 0",
				     me.name, chptr->chname, chptr->chname, (long) oldts);
		sendto_realops_snomask(SNO_GENERAL, L_ALL,
				     "Server %s changing TS on %s from %ld to 0",
				     source_p->name, chptr->chname, (long) oldts);
	}

	if(isnew)
		chptr->channelts = newts;

	else if(newts == 0 || oldts == 0)
		chptr->channelts = 0;
	else if(newts == oldts)
		;
	else if(newts < oldts)
	{
		/* If configured, kick people trying to join +i/+k
		 * channels by recreating them on split servers.
		 * If the source has sent EOB, assume this is some
		 * sort of hack by services. If cmode +i is set,
		 * services can send kicks if needed; if the key
		 * differs, services cannot kick in a race-free
		 * manner so do so here.
		 * -- jilles */
		if (ConfigChannel.kick_on_split_riding &&
				((!HasSentEob(source_p) &&
				mode.mode & MODE_INVITEONLY) ||
		    (mode.key[0] != 0 && irccmp(mode.key, oldmode->key) != 0)))
		{
			struct membership *msptr;
			struct Client *who;
			int l = rb_dlink_list_length(&chptr->members);

			RB_DLINK_FOREACH_SAFE(ptr, next_ptr, chptr->locmembers.head)
			{
				msptr = ptr->data;
				who = msptr->client_p;
				sendto_one(who, ":%s KICK %s %s :Net Rider",
						     me.name, chptr->chname, who->name);

				sendto_server(NULL, chptr, CAP_TS6, NOCAPS,
					      ":%s KICK %s %s :Net Rider",
					      me.id, chptr->chname,
					      who->id);
				remove_user_from_channel(msptr);
				if (--l == 0)
					break;
			}
			if (l == 0)
			{
				/* Channel was emptied, create a new one */
				if((chptr = get_or_create_channel(source_p, parv[2], &isnew)) == NULL)
					return;		/* oops! */

				oldmode = &chptr->mode;
			}
		}
		keep_our_modes = false;
		chptr->channelts = newts;
	}
	else
		keep_new_modes = false;

	if(!keep_new_modes)
		mode = *oldmode;
	else if(keep_our_modes)
	{
		mode.mode |= oldmode->mode;
		if(oldmode->limit > mode.limit)
			mode.limit = oldmode->limit;
		if(strcmp(mode.key, oldmode->key) < 0)
			rb_strlcpy(mode.key, oldmode->key, sizeof(mode.key));
		if(oldmode->join_num > mode.join_num ||
				(oldmode->join_num == mode.join_num &&
				 oldmode->join_time > mode.join_time))
		{
			mode.join_num = oldmode->join_num;
			mode.join_time = oldmode->join_time;
		}
		if(irccmp(mode.forward, oldmode->forward) < 0)
			rb_strlcpy(mode.forward, oldmode->forward, sizeof(mode.forward));
	}
	else
	{
		/* If setting -j, clear join throttle state -- jilles */
		if (!mode.join_num)
			chptr->join_count = chptr->join_delta = 0;
	}

	mbuf = set_final_mode(mbuf, parabuf, &mode, oldmode);
	chptr->mode = mode;

	/* Lost the TS, other side wins, so remove modes on this side */
	if(!keep_our_modes)
	{
		remove_our_modes(chptr, fakesource_p);
		RB_DLINK_FOREACH_SAFE(ptr, next_ptr, chptr->invites.head)
		{
			del_invite(chptr, ptr->data);
		}

		if(rb_dlink_list_length(&chptr->banlist) > 0)
			remove_ban_list(chptr, fakesource_p, &chptr->banlist, 'b', ALL_MEMBERS);
		if(rb_dlink_list_length(&chptr->exceptlist) > 0)
			remove_ban_list(chptr, fakesource_p, &chptr->exceptlist,
					'e', ONLY_CHANOPS);
		if(rb_dlink_list_length(&chptr->invexlist) > 0)
			remove_ban_list(chptr, fakesource_p, &chptr->invexlist,
					'I', ONLY_CHANOPS);
		if(rb_dlink_list_length(&chptr->quietlist) > 0)
			remove_ban_list(chptr, fakesource_p, &chptr->quietlist,
					'q', ALL_MEMBERS);
		chptr->bants++;

		sendto_channel_local(&me, ALL_MEMBERS, chptr,
				     ":%s NOTICE %s :*** Notice -- TS for %s changed from %ld to %ld",
				     me.name, chptr->chname, chptr->chname,
				     (long) oldts, (long) newts);
		/* Update capitalization in channel name, this makes the
		 * capitalization timestamped like modes are -- jilles */
		strcpy(chptr->chname, parv[2]);

		/* since we're dropping our modes, we want to clear the mlock as well. --nenolod */
		set_channel_mlock(client_p, source_p, chptr, NULL, false);
	}

	if(*modebuf != '\0')
		sendto_channel_local(fakesource_p, ALL_MEMBERS, chptr, ":%s MODE %s %s %s",
				     fakesource_p->name, chptr->chname, modebuf, parabuf);

	*modebuf = *parabuf = '\0';

	if(parv[3][0] != '0' && keep_new_modes)
		modes = channel_modes(chptr, source_p);
	else
		modes = empty_modes;

	mlen_uid = sprintf(buf_uid, ":%s SJOIN %ld %s %s :",
			      use_id(source_p), (long) chptr->channelts, parv[2], modes);
	ptr_uid = buf_uid + mlen_uid;

	mbuf = modebuf;
	para[0] = para[1] = para[2] = para[3] = empty;
	pargs = 0;
	len_uid = 0;

	/* if theres a space, theres going to be more than one nick, change the
	 * first space to \0, so s is just the first nick, and point p to the
	 * second nick
	 */
	if((p = strchr(s, ' ')) != NULL)
	{
		*p++ = '\0';
	}

	*mbuf++ = '+';

	while (s)
	{
		fl = 0;

		for (i = 0; i < 2; i++)
		{
			if(*s == '@')
			{
				fl |= CHFL_CHANOP;
				s++;
			}
			else if(*s == '+')
			{
				fl |= CHFL_VOICE;
				s++;
			}
		}

		/* if the client doesnt exist or is fake direction, skip. */
		if(!(target_p = find_client(s)) ||
		   (target_p->from != client_p) || !IsPerson(target_p))
			goto nextnick;

		/* we assume for these we can fit at least one nick/uid in.. */

		/* check we can fit another status+nick+space into a buffer */
		if((mlen_uid + len_uid + IDLEN + 3) > (BUFSIZE - 3))
		{
			*(ptr_uid - 1) = '\0';
			sendto_server(client_p->from, NULL, CAP_TS6, NOCAPS, "%s", buf_uid);
			ptr_uid = buf_uid + mlen_uid;
			len_uid = 0;
		}

		if(keep_new_modes)
		{
			if(fl & CHFL_CHANOP)
			{
				*ptr_uid++ = '@';
				len_uid++;
			}
			if(fl & CHFL_VOICE)
			{
				*ptr_uid++ = '+';
				len_uid++;
			}
		}

		/* copy the nick to the two buffers */
		len = sprintf(ptr_uid, "%s ", use_id(target_p));
		ptr_uid += len;
		len_uid += len;

		if(!keep_new_modes)
			fl = 0;

		if(!IsMember(target_p, chptr))
		{
			add_user_to_channel(chptr, target_p, fl);
			send_channel_join(chptr, target_p);
			joins++;
		}

		if(fl & CHFL_CHANOP)
		{
			*mbuf++ = 'o';
			para[pargs++] = target_p->name;

			/* a +ov user.. bleh */
			if(fl & CHFL_VOICE)
			{
				/* its possible the +o has filled up MAXMODEPARAMS, if so, start
				 * a new buffer
				 */
				if(pargs >= MAXMODEPARAMS)
				{
					*mbuf = '\0';
					sendto_channel_local(fakesource_p, ALL_MEMBERS, chptr,
							     ":%s MODE %s %s %s %s %s %s",
							     fakesource_p->name, chptr->chname,
							     modebuf,
							     para[0], para[1], para[2], para[3]);
					mbuf = modebuf;
					*mbuf++ = '+';
					para[0] = para[1] = para[2] = para[3] = NULL;
					pargs = 0;
				}

				*mbuf++ = 'v';
				para[pargs++] = target_p->name;
			}
		}
		else if(fl & CHFL_VOICE)
		{
			*mbuf++ = 'v';
			para[pargs++] = target_p->name;
		}

		if(pargs >= MAXMODEPARAMS)
		{
			*mbuf = '\0';
			sendto_channel_local(fakesource_p, ALL_MEMBERS, chptr,
					     ":%s MODE %s %s %s %s %s %s",
					     fakesource_p->name,
					     chptr->chname,
					     modebuf, para[0], para[1], para[2], para[3]);
			mbuf = modebuf;
			*mbuf++ = '+';
			para[0] = para[1] = para[2] = para[3] = NULL;
			pargs = 0;
		}

	      nextnick:
		/* p points to the next nick */
		s = p;

		/* if there was a trailing space and p was pointing to it, then we
		 * need to exit.. this has the side effect of breaking double spaces
		 * in an sjoin.. but that shouldnt happen anyway
		 */
		if(s && (*s == '\0'))
			s = p = NULL;

		/* if p was NULL due to no spaces, s wont exist due to the above, so
		 * we cant check it for spaces.. if there are no spaces, then when
		 * we next get here, s will be NULL
		 */
		if(s && ((p = strchr(s, ' ')) != NULL))
		{
			*p++ = '\0';
		}
	}

	*mbuf = '\0';
	if(pargs)
	{
		sendto_channel_local(fakesource_p, ALL_MEMBERS, chptr,
				     ":%s MODE %s %s %s %s %s %s",
				     fakesource_p->name, chptr->chname, modebuf,
				     para[0], CheckEmpty(para[1]),
				     CheckEmpty(para[2]), CheckEmpty(para[3]));
	}

	if(!joins && !(chptr->mode.mode & MODE_PERMANENT) && isnew)
	{
		destroy_channel(chptr);

		return;
	}

	/* Keep the colon if we're sending an SJOIN without nicks -- jilles */
	if (joins)
	{
		*(ptr_uid - 1) = '\0';
	}

	sendto_server(client_p->from, NULL, CAP_TS6, NOCAPS, "%s", buf_uid);
}

/*
 * do_join_0
 *
 * inputs	- pointer to client doing join 0
 * output	- NONE
 * side effects	- Use has decided to join 0. This is legacy
 *		  from the days when channels were numbers not names. *sigh*
 */
static void
do_join_0(struct Client *client_p, struct Client *source_p)
{
	struct membership *msptr;
	struct Channel *chptr = NULL;
	rb_dlink_node *ptr;

	/* Finish the flood grace period... */
	if(MyClient(source_p) && !IsFloodDone(source_p))
		flood_endgrace(source_p);

	sendto_server(client_p, NULL, CAP_TS6, NOCAPS, ":%s JOIN 0", use_id(source_p));

	while((ptr = source_p->user->channel.head))
	{
		if(MyConnect(source_p) &&
		   !IsOperGeneral(source_p) && !IsExemptSpambot(source_p))
			check_spambot_warning(source_p, NULL);

		msptr = ptr->data;
		chptr = msptr->chptr;
		sendto_channel_local(source_p, ALL_MEMBERS, chptr, ":%s!%s@%s PART %s",
				     source_p->name,
				     source_p->username, source_p->host, chptr->chname);
		remove_user_from_channel(msptr);
	}
}

static bool
check_channel_name_loc(struct Client *source_p, const char *name)
{
	const char *p;

	s_assert(name != NULL);
	if(EmptyString(name))
		return false;

	if(ConfigFileEntry.disable_fake_channels && !IsOperGeneral(source_p))
	{
		for(p = name; *p; ++p)
		{
			if(!IsChanChar(*p) || IsFakeChanChar(*p))
				return false;
		}
	}
	else
	{
		for(p = name; *p; ++p)
		{
			if(!IsChanChar(*p))
				return false;
		}
	}

	if(ConfigChannel.only_ascii_channels)
	{
		for(p = name; *p; ++p)
			if(*p < 33 || *p > 126)
				return false;
	}

	return true;
}

/* send_join_error()
 *
 * input	- client to send to, reason, channel name
 * output	- none
 * side effects - error message sent to client
 */
static void
send_join_error(struct Client *source_p, int numeric, const char *name)
{
	/* This stuff is necessary because the form_str macro only
	 * accepts constants.
	 */
	switch (numeric)
	{
#define NORMAL_NUMERIC(i)						\
		case i:							\
			sendto_one(source_p, form_str(i),		\
					me.name, source_p->name, name);	\
			break

		NORMAL_NUMERIC(ERR_BANNEDFROMCHAN);
		NORMAL_NUMERIC(ERR_INVITEONLYCHAN);
		NORMAL_NUMERIC(ERR_BADCHANNELKEY);
		NORMAL_NUMERIC(ERR_CHANNELISFULL);
		NORMAL_NUMERIC(ERR_NEEDREGGEDNICK);
		NORMAL_NUMERIC(ERR_THROTTLE);

		case ERR_USERONCHANNEL:
			sendto_one_numeric(source_p, ERR_USERONCHANNEL,
					form_str(ERR_USERONCHANNEL), source_p->name, name);
			break;
		default:
			sendto_one_numeric(source_p, numeric,
					"%s :Cannot join channel", name);
			break;
	}
}

static char *
set_final_mode(char *mbuf, char *parabuf, struct Mode *mode, struct Mode *oldmode)
{
	int dir = MODE_QUERY;
	char *pbuf = parabuf;
	int len;
	int i;

	/* ok, first get a list of modes we need to add */
	for (i = 0; i < 256; i++)
	{
		if((mode->mode & chmode_flags[i]) && !(oldmode->mode & chmode_flags[i]))
		{
			if(dir != MODE_ADD)
			{
				*mbuf++ = '+';
				dir = MODE_ADD;
			}
			*mbuf++ = i;
		}
	}

	/* now the ones we need to remove. */
	for (i = 0; i < 256; i++)
	{
		if((oldmode->mode & chmode_flags[i]) && !(mode->mode & chmode_flags[i]))
		{
			if(dir != MODE_DEL)
			{
				*mbuf++ = '-';
				dir = MODE_DEL;
			}
			*mbuf++ = i;
		}
	}

	if(oldmode->limit && !mode->limit)
	{
		if(dir != MODE_DEL)
		{
			*mbuf++ = '-';
			dir = MODE_DEL;
		}
		*mbuf++ = 'l';
	}
	if(oldmode->key[0] && !mode->key[0])
	{
		if(dir != MODE_DEL)
		{
			*mbuf++ = '-';
			dir = MODE_DEL;
		}
		*mbuf++ = 'k';
		len = sprintf(pbuf, "%s ", oldmode->key);
		pbuf += len;
	}
	if(oldmode->join_num && !mode->join_num)
	{
		if(dir != MODE_DEL)
		{
			*mbuf++ = '-';
			dir = MODE_DEL;
		}
		*mbuf++ = 'j';
	}
	if(oldmode->forward[0] && !mode->forward[0])
	{
		if(dir != MODE_DEL)
		{
			*mbuf++ = '-';
			dir = MODE_DEL;
		}
		*mbuf++ = 'f';
	}
	if(mode->limit && oldmode->limit != mode->limit)
	{
		if(dir != MODE_ADD)
		{
			*mbuf++ = '+';
			dir = MODE_ADD;
		}
		*mbuf++ = 'l';
		len = sprintf(pbuf, "%d ", mode->limit);
		pbuf += len;
	}
	if(mode->key[0] && strcmp(oldmode->key, mode->key))
	{
		if(dir != MODE_ADD)
		{
			*mbuf++ = '+';
			dir = MODE_ADD;
		}
		*mbuf++ = 'k';
		len = sprintf(pbuf, "%s ", mode->key);
		pbuf += len;
	}
	if(mode->join_num && (oldmode->join_num != mode->join_num || oldmode->join_time != mode->join_time))
	{
		if(dir != MODE_ADD)
		{
			*mbuf++ = '+';
			dir = MODE_ADD;
		}
		*mbuf++ = 'j';
		len = sprintf(pbuf, "%d:%d ", mode->join_num, mode->join_time);
		pbuf += len;
	}
	if(mode->forward[0] && strcmp(oldmode->forward, mode->forward) &&
			ConfigChannel.use_forward)
	{
		if(dir != MODE_ADD)
		{
			*mbuf++ = '+';
			dir = MODE_ADD;
		}
		*mbuf++ = 'f';
		len = sprintf(pbuf, "%s ", mode->forward);
		pbuf += len;
	}
	*mbuf = '\0';

	return mbuf;
}

/*
 * remove_our_modes
 *
 * inputs	-
 * output	-
 * side effects	-
 */
static void
remove_our_modes(struct Channel *chptr, struct Client *source_p)
{
	struct membership *msptr;
	rb_dlink_node *ptr;
	char lmodebuf[MODEBUFLEN];
	char *lpara[MAXMODEPARAMS];
	int count = 0;
	int i;
	char *mbuf = lmodebuf;

	*mbuf++ = '-';

	for(i = 0; i < MAXMODEPARAMS; i++)
		lpara[i] = NULL;

	RB_DLINK_FOREACH(ptr, chptr->members.head)
	{
		msptr = ptr->data;

		if(is_chanop(msptr))
		{
			msptr->flags &= ~CHFL_CHANOP;
			lpara[count++] = msptr->client_p->name;
			*mbuf++ = 'o';

			/* +ov, might not fit so check. */
			if(is_voiced(msptr))
			{
				if(count >= MAXMODEPARAMS)
				{
					*mbuf = '\0';
					sendto_channel_local(source_p, ALL_MEMBERS, chptr,
							     ":%s MODE %s %s %s %s %s %s",
							     source_p->name, chptr->chname,
							     lmodebuf, lpara[0], lpara[1],
							     lpara[2], lpara[3]);

					/* preserve the initial '-' */
					mbuf = lmodebuf;
					*mbuf++ = '-';
					count = 0;

					for(i = 0; i < MAXMODEPARAMS; i++)
						lpara[i] = NULL;
				}

				msptr->flags &= ~CHFL_VOICE;
				lpara[count++] = msptr->client_p->name;
				*mbuf++ = 'v';
			}
		}
		else if(is_voiced(msptr))
		{
			msptr->flags &= ~CHFL_VOICE;
			lpara[count++] = msptr->client_p->name;
			*mbuf++ = 'v';
		}
		else
			continue;

		if(count >= MAXMODEPARAMS)
		{
			*mbuf = '\0';
			sendto_channel_local(source_p, ALL_MEMBERS, chptr,
					     ":%s MODE %s %s %s %s %s %s",
					     source_p->name, chptr->chname, lmodebuf,
					     lpara[0], lpara[1], lpara[2], lpara[3]);
			mbuf = lmodebuf;
			*mbuf++ = '-';
			count = 0;

			for(i = 0; i < MAXMODEPARAMS; i++)
				lpara[i] = NULL;
		}
	}

	if(count != 0)
	{
		*mbuf = '\0';
		sendto_channel_local(source_p, ALL_MEMBERS, chptr,
				     ":%s MODE %s %s %s %s %s %s",
				     source_p->name, chptr->chname, lmodebuf,
				     EmptyString(lpara[0]) ? "" : lpara[0],
				     EmptyString(lpara[1]) ? "" : lpara[1],
				     EmptyString(lpara[2]) ? "" : lpara[2],
				     EmptyString(lpara[3]) ? "" : lpara[3]);

	}
}

/* remove_ban_list()
 *
 * inputs	- channel, source, list to remove, char of mode, caps needed
 * outputs	-
 * side effects - given list is removed, with modes issued to local clients
 */
static void
remove_ban_list(struct Channel *chptr, struct Client *source_p,
		rb_dlink_list * list, char c, int mems)
{
	static char lmodebuf[BUFSIZE];
	static char lparabuf[BUFSIZE];
	struct Ban *banptr;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	char *pbuf;
	int count = 0;
	int cur_len, mlen, plen;
	char *mbuf;

	pbuf = lparabuf;

	cur_len = mlen = sprintf(lmodebuf, ":%s MODE %s -", source_p->name, chptr->chname);
	mbuf = lmodebuf + mlen;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, list->head)
	{
		banptr = ptr->data;

		/* trailing space, and the mode letter itself */
		plen = strlen(banptr->banstr) +
			(banptr->forward ? strlen(banptr->forward) + 1 : 0) + 2;

		if(count >= MAXMODEPARAMS || (cur_len + plen) > BUFSIZE - 4)
		{
			/* remove trailing space */
			*mbuf = '\0';
			*(pbuf - 1) = '\0';

			sendto_channel_local(source_p, mems, chptr, "%s %s", lmodebuf, lparabuf);

			cur_len = mlen;
			mbuf = lmodebuf + mlen;
			pbuf = lparabuf;
			count = 0;
		}

		*mbuf++ = c;
		cur_len += plen;
		if (banptr->forward)
			pbuf += sprintf(pbuf, "%s$%s ", banptr->banstr, banptr->forward);
		else
			pbuf += sprintf(pbuf, "%s ", banptr->banstr);
		count++;

		free_ban(banptr);
	}

	*mbuf = '\0';
	*(pbuf - 1) = '\0';
	sendto_channel_local(source_p, mems, chptr, "%s %s", lmodebuf, lparabuf);

	list->head = list->tail = NULL;
	list->length = 0;
}
