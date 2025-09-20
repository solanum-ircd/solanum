/*
 *  ircd-ratbox: A slightly useful ircd.
 *  send.h: A header for the message sending functions.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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

#ifndef INCLUDED_send_h
#define INCLUDED_send_h

#include "rb_lib.h"
#include "ircd_defs.h"
#include "msgbuf.h"

struct Client;
struct Channel;
struct monitor;

/* The nasty global also used in s_serv.c for server bursts */
extern unsigned long current_serial;

extern struct Client *remote_rehash_oper_p;

extern void send_pop_queue(struct Client *);

extern void send_queued(struct Client *to);

extern void sendto_one(struct Client *target_p, const char *, ...) AFP(2, 3);
extern void sendto_one_notice(struct Client *target_p,const char *, ...) AFP(2, 3);
extern void sendto_one_prefix(struct Client *target_p, struct Client *source_p,
			      const char *command, const char *, ...) AFP(4, 5);
extern void sendto_one_numeric(struct Client *target_p,
			       int numeric, const char *, ...) AFP(3, 4);
extern void sendto_one_tags(struct Client *target_p, int serv_cap, int serv_negcap,
	size_t n_tags, const struct MsgTag tags[], const char *, ...) AFP(6, 7);

extern void sendto_server(struct Client *one, struct Channel *chptr,
			  unsigned long caps, unsigned long nocaps,
			  const char *format, ...) AFP(5, 6);

extern void sendto_channel_flags(struct Client *one, int type, struct Client *source_p,
				 struct Channel *chptr, const char *, ...) AFP(5, 6);
extern void sendto_channel_flags_tags(struct Client *one, int type, struct Client *source_p,
				 struct Channel *chptr, int cli_cap, int cli_negcap, int serv_cap, int serv_negcap,
				 size_t n_tags, const struct MsgTag tags[], const char *, ...) AFP(11, 12);
extern void sendto_channel_opmod(struct Client *one, struct Client *source_p,
				 struct Channel *chptr, const char *command, const char *text);
extern void sendto_channel_opmod_tags(struct Client *one, struct Client *source_p,
				 struct Channel *chptr, int cli_cap, int cli_negcap, int serv_cap, int serv_negcap,
				 const char *command, const char *text, size_t n_tags, const struct MsgTag tags[]);

extern void sendto_channel_local(struct Client *, int type, struct Channel *, const char *, ...) AFP(4, 5);
extern void sendto_channel_local_priv(struct Client *, int type, const char *priv, struct Channel *, const char *, ...) AFP(5, 6);
extern void sendto_channel_local_tags(struct Client *source_p, int type, const char *priv, struct Channel *chptr,
	size_t n_tags, const struct MsgTag tags[], const char *, ...) AFP (7, 8);
extern void sendto_channel_local_butone(struct Client *, int type, struct Channel *, const char *, ...) AFP(4, 5);

extern void sendto_channel_local_with_capability(struct Client *, int type, int caps, int negcaps, struct Channel *, const char *, ...) AFP(6, 7);
extern void sendto_channel_local_with_capability_butone(struct Client *, int type, int caps, int negcaps, struct Channel *,
							const char *, ...) AFP(6, 7);
extern void sendto_channel_local_with_capability_butone_tags(struct Client *, int type, int caps, int negcaps, struct Channel *,
							size_t n_tags, const struct MsgTag tags[], const char *, ...) AFP(8, 9);

extern void sendto_common_channels_local(struct Client *, int cap, int negcap, const char *, ...) AFP(4, 5);
extern void sendto_common_channels_local_butone(struct Client *, int cap, int negcap, const char *, ...) AFP(4, 5);


extern void sendto_match_butone(struct Client *one, struct Client *source_p,
				const char *mask, int what, const char *, ...) AFP(5, 6);
extern void sendto_match_butone_tags(struct Client *one, struct Client *source_p,
				const char *mask, int what, int cli_cap, int cli_negcap, int serv_cap, int serv_negcap,
				size_t n_tags, const struct MsgTag tags[], const char *, ...) AFP(11, 12);
extern void sendto_match_servs(struct Client *source_p, const char *mask,
				int capab, int, const char *, ...) AFP(5, 6);

extern void sendto_monitor(struct Client *, struct monitor *monptr, const char *, ...) AFP(3, 4);

extern void sendto_anywhere(struct Client *, struct Client *, const char *,
			    const char *, ...) AFP(4, 5);
extern void sendto_anywhere_echo(struct Client *, struct Client *, const char *,
			    const char *, ...) AFP(4, 5);
extern void sendto_anywhere_tags(struct Client *target_p, struct Client *source_p, const char *command,
	int serv_cap, int serv_negcap, size_t n_tags, const struct MsgTag tags[], const char *pattern, ...) AFP(8, 9);
extern void sendto_local_clients_with_capability(int cap, const char *pattern, ...) AFP(2, 3);

extern void sendto_realops_snomask(int, int, const char *, ...) AFP(3, 4);
extern void sendto_realops_snomask_from(int, int, struct Client *, const char *, ...) AFP(4, 5);

extern void sendto_wallops_flags(int, struct Client *, const char *, ...) AFP(3, 4);

extern void kill_client(struct Client *client_p, struct Client *diedie,
			 const char *pattern, ...) AFP(3, 4);
extern void kill_client_serv_butone(struct Client *one, struct Client *source_p,
				    const char *pattern, ...) AFP(3, 4);

enum multiline_item_result {
	MULTILINE_FAILURE,
	MULTILINE_SUCCESS,
	MULTILINE_WRAPPED,
};

extern bool send_multiline_init(struct Client *target_p, const char *separator, const char *format, ...) AFP(3, 4);
extern bool send_multiline_remote_pad(struct Client *target_p, struct Client *client_p);
extern enum multiline_item_result send_multiline_item(struct Client *target_p, const char *format, ...) AFP(2, 3);
extern bool send_multiline_fini(struct Client *target_p, const char *format, ...) AFP(2, 3);
extern void send_multiline_reset(void);

#define L_ALL 	0
#define L_OPER 	1
#define L_ADMIN	2
#define L_NETWIDE 256 /* OR with L_ALL or L_OPER */

#define NOCAPS          0	/* no caps */

/* used when sending to #mask or $mask */
#define MATCH_SERVER  1
#define MATCH_HOST    2

#endif /* INCLUDED_send_h */
