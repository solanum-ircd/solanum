/*
 * Solanum: a slightly advanced ircd
 * response.h: header for labeled-response support
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

#ifndef INCLUDED_response_h
#define INCLUDED_response_h

struct Client;

struct ResponseInfo
{
	/* local client to reach source_p by */
	struct Client *client_p;
	/* client to send the labeled-response to */
	struct Client *source_p;
	/* client-specified label; empty string if none specified or source_p is remote */
	char label[65];
	/* the batch id of the response batch; empty string if not batched */
	char batch[20];
	/* number of remote servers we are expecting to receive responses from */
	int remote_response;
	/* if remote_response is nonzero, time that we stop waiting for a remote response and close the batch */
	time_t expires;
	/* whether a response has already been sent to the local client */
	bool sent;
};

/* state of an outgoing labeled-response */
extern struct ResponseInfo *outgoing_response_info;

/* initialize labeled-response memory */
void init_response(void);

/* send a labeled-response BATCH to the incoming client for replies that will be generated entirely locally */
void begin_local_response_batch(void);

/* send a labeled-response BATCH to the incoming client for replies that will be generated at least partially
 * by remote servers. server_count must be the number of servers expected to send responses back
 */
void begin_remote_response_batch(int server_count);

/* Get details for a labeled-response batch with the specified batch ID */
struct ResponseInfo *get_remote_response_batch(const char *batch);

/* Free memory and clean up references to a pending labeled-response batch */
void free_response_batch(struct ResponseInfo *response);

/* Count the number of servers matching a given glob mask and capabilities, except for servers in the direction of one
 * This helps provide an accurate count for begin_remote_response_batch() when broadcasting to multiple servers
 */
int count_match_servs(struct Client *one, const char *mask, uint64_t cap, uint64_t negcap);

#endif /* INCLUDED_response_h */
