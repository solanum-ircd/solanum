/*
 *  solanum: An advanced IRCd.
 *  ircd_defs.h: A header for ircd global definitions.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2005-2006 Charybdis development team
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

/*
 * NOTE: NICKLEN and TOPICLEN do not live here anymore. Set it with configure
 * Otherwise there are no user servicable part here.
 */

/* ircd_defs.h - Global size definitions for record entries used
 * througout ircd. Please think 3 times before adding anything to this
 * file.
 */
#ifndef INCLUDED_ircd_defs_h
#define INCLUDED_ircd_defs_h

#include "defaults.h"

/* For those unfamiliar with GNU format attributes, a is the 1 based
 * argument number of the format string, and b is the 1 based argument
 * number of the variadic ... */
#ifdef __GNUC__
#define AFP(a,b) __attribute__((format (printf, a, b)))
#else
#define AFP(a,b)
#endif

/*
 * This ensures that __attribute__((deprecated)) is not used in for example
 * sun CC, since it's a GNU-specific extension. -nenolod
 */
#ifdef __GNUC__
#define IRC_DEPRECATED __attribute__((deprecated))
#else
#define IRC_DEPRECATED
#endif

#ifndef MAX
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

#define ARRAY_SIZE(array)       (sizeof(array) / sizeof((array)[0]))

#define HOSTLEN         63	/* Length of hostname.  Updated to         */
				/* comply with RFC1123                     */

/* Longest hostname we're willing to work with.
 * Due to DNSBLs this is more than HOSTLEN.
 */
#define IRCD_RES_HOSTLEN 255

#define USERLEN         10
#define REALLEN         50
#define CHANNELLEN      200
#define LOC_CHANNELLEN	50

/* reason length of klines, parts, quits etc */
/* for quit messages, note that a client exit server notice
 * :012345678901234567890123456789012345678901234567890123456789123 NOTICE * :*** Notice -- Client exiting: 012345678901234567 (0123456789@012345678901234567890123456789012345678901234567890123456789123) [] [1111:2222:3333:4444:5555:6666:7777:8888]
 * takes at most 246 bytes (including CRLF and '\0') and together with the
 * quit reason should fit in 512 */
#define REASONLEN	260	/* kick/part/quit */
#define BANREASONLEN	390	/* kline/dline */
#define AWAYLEN		TOPICLEN
#define KILLLEN         200	/* with Killed (nick ()) added this should fit in quit */

/* 23+1 for \0 */
#define KEYLEN          24
#define TAGSLEN         8191	/* IRCv3 message tags */
#define TAGSPARTLEN     4094	/* maximum tag data for either client or server tags */
#define DATALEN         510	/* RFC1459 message data */
#define BUFSIZE         512	/* WARNING: *DONT* CHANGE THIS!!!! */
#define EXT_BUFSIZE     (TAGSLEN + DATALEN + 1)
#define OPERNICKLEN     (NICKLEN*2)	/* Length of OPERNICKs. */

#define NAMELEN	        (MAX(NICKLEN, HOSTLEN))

#define USERHOST_REPLYLEN       (NAMELEN+HOSTLEN+USERLEN+5)
#define MAX_DATE_STRING 32	/* maximum string length for a date string */

#define HELPLEN         400

/*
 * message return values
 */
#define CLIENT_EXITED    -2
#define CLIENT_PARSE_ERROR -1
#define CLIENT_OK	1

#ifndef AF_INET6
#error "AF_INET6 not defined"
#endif

#define PATRICIA_BITS	128

/* Read buffer size */
#define READBUF_SIZE 16384

#endif /* INCLUDED_ircd_defs_h */
