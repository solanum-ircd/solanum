/*
 *  ircd-ratbox: A slightly useful ircd.
 *  s_serv.h: A header for the server functions.
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

#ifndef INCLUDED_serv_h
#define INCLUDED_serv_h

#include "defaults.h"
#include "capability.h"

/*
 * The number of seconds between calls to try_connections(). Fiddle with
 * this ONLY if you KNOW what you're doing!
 */
#define TRY_CONNECTIONS_TIME	60

/*
 * number of seconds to wait after server starts up, before
 * starting try_connections()
 * TOO SOON and you can nick collide like crazy.
 */
#define STARTUP_CONNECTIONS_TIME 60

struct Client;
struct server_conf;
struct Channel;

/* Capabilities */
extern struct CapabilityIndex *serv_capindex;
extern struct CapabilityIndex *cli_capindex;

/* register client capabilities with this structure for 3.2 enhanced capability negotiation */
#define CLICAP_FLAGS_STICKY    0x001
#define CLICAP_FLAGS_PRIORITY  0x002

struct ClientCapability {
	bool (*visible)(struct Client *);		/* whether or not to display the capability.  set to NULL or true return value = displayed */
	const char *(*data)(struct Client *);		/* any custom data for the capability.  set to NULL or return NULL = no data */
	unsigned int flags;
};

/* builtin client capabilities */
extern unsigned int CLICAP_MULTI_PREFIX;
extern unsigned int CLICAP_ACCOUNT_NOTIFY;
extern unsigned int CLICAP_EXTENDED_JOIN;
extern unsigned int CLICAP_AWAY_NOTIFY;
extern unsigned int CLICAP_USERHOST_IN_NAMES;
extern unsigned int CLICAP_CAP_NOTIFY;
extern unsigned int CLICAP_CHGHOST;
extern unsigned int CLICAP_ECHO_MESSAGE;
extern unsigned int CLICAP_EXTENDED_MONITOR;

/*
 * XXX: this is kind of ugly, but this allows us to have backwards
 * API-compatibility.
 */
extern unsigned int CAP_CAP;			/* received a CAP to begin with */
extern unsigned int CAP_QS;			/* Can handle quit storm removal */
extern unsigned int CAP_EX;			/* Can do channel +e exemptions */
extern unsigned int CAP_CHW;			/* Can do channel wall @# */
extern unsigned int CAP_IE;			/* Can do invite exceptions */
extern unsigned int CAP_KLN;			/* Can do KLINE message */
extern unsigned int CAP_KNOCK;			/* supports KNOCK */
extern unsigned int CAP_TB;			/* supports TBURST */
extern unsigned int CAP_UNKLN;			/* supports remote unkline */
extern unsigned int CAP_CLUSTER;		/* supports cluster stuff */
extern unsigned int CAP_ENCAP;			/* supports ENCAP */
extern unsigned int CAP_TS6;			/* supports TS6 or above */
extern unsigned int CAP_SERVICE;		/* supports services */
extern unsigned int CAP_RSFNC;			/* rserv FNC */
extern unsigned int CAP_SAVE;			/* supports SAVE (nick collision FNC) */
extern unsigned int CAP_EUID;			/* supports EUID (ext UID + nonencap CHGHOST) */
extern unsigned int CAP_EOPMOD;			/* supports EOPMOD (ext +z + ext topic) */
extern unsigned int CAP_BAN;			/* supports propagated bans */
extern unsigned int CAP_MLOCK;			/* supports MLOCK messages */
extern unsigned int CAP_EBMASK;			/* supports sending BMASK set by/at metadata */

/* XXX: added for backwards compatibility. --nenolod */
#define CAP_MASK	(capability_index_mask(serv_capindex) & ~(CAP_TS6 | CAP_CAP))

/*
 * Capability macros.
 */
#define IsCapable(x, cap)       (((x)->localClient->caps & (cap)) == cap)
#define NotCapable(x, cap)	(((x)->localClient->caps & (cap)) == 0)
#define ClearCap(x, cap)        ((x)->localClient->caps &= ~(cap))

/*
 * Globals
 *
 *
 * list of recognized server capabilities.  "TS" is not on the list
 * because all servers that we talk to already do TS, and the kludged
 * extra argument to "PASS" takes care of checking that.  -orabidoo
 */
extern int MaxClientCount;	/* GLOBAL - highest number of clients */
extern int MaxConnectionCount;	/* GLOBAL - highest number of connections */

extern int refresh_user_links;

/*
 * return values for hunt_server()
 */
#define HUNTED_NOSUCH   (-1)	/* if the hunted server is not found */
#define HUNTED_ISME     0	/* if this server should execute the command */
#define HUNTED_PASS     1	/* if message passed onwards successfully */

extern void init_builtin_capabs(void);

extern int hunt_server(struct Client *client_pt,
		       struct Client *source_pt,
		       const char *command, int server, int parc, const char **parv);
extern void send_capabilities(struct Client *, unsigned int);
extern const char *show_capabilities(struct Client *client);
extern void try_connections(void *unused);

extern int check_server(const char *name, struct Client *server);
extern int server_estab(struct Client *client_p);

extern int serv_connect(struct server_conf *, struct Client *);

#endif /* INCLUDED_s_serv_h */
