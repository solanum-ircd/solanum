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

/* indicates the ResponseInfo is statically allocated and free_response_batch should no-op on it */
#define RESPONSE_FLAG_STATIC    0x01
/* indicates that the label tag has already been sent for this ResponseInfo */
#define RESPONSE_FLAG_SENT      0x02
/* indicates that the ResponseInfo should not automatically expire */
#define RESPONSE_FLAG_NO_EXPIRE 0x04

struct Client;

struct ResponseInfo
{
	/* local client to reach source_p by */
	struct Client *client_p;
	/* client to send the labeled-response to */
	struct Client *source_p;
	/* client-specified label; NULL if none specified or source_p is remote */
	char *label;
	/* the batch id of the response batch; NULL if not batched */
	char *batch;
	/* number of remote servers we are expecting to receive responses from */
	int remote_response;
	/* time that we stop waiting for a remote response and close the batch */
	time_t expires;
	/* the node in source_p->localClient->pending_remote_responses (may be NULL) */
	rb_dlink_node *remote_node;
	/* for remote batches, the server mask dictating which servers we are expecting responses from */
	char *mask;
	/* miscellaneous flags for this ResponseInfo */
	uint32_t flags;
};

/* state of an outgoing labeled-response */
extern struct ResponseInfo *outgoing_response_info;

/* initialize labeled-response memory */
void init_response(void);

/* send a labeled-response BATCH to the incoming client for replies that will be generated entirely locally.
 * No-op if a batch is already opened.
 */
void begin_local_response_batch(void);

/* send a labeled-response BATCH to the incoming client for replies that will be generated at least partially
 * by remote servers. server_count must be the number of servers expected to send responses back.
 * mask must be a string (potentially containing * and ? wildcards) that matches servers we need responses from.
 * If a local batch is already opened, transforms it into a remote batch.
 * Calling on an existing remote batch is an error.
 */
void begin_remote_response_batch(int server_count, const char *mask);

/* Resume an already-began response batch (generally saved away somewhere else).
 * The previously active response is returned (NULL if no previously active response).
 * Any caller making use of this function *MUST* call resume_response_batch or free_response_batch,
 * passing the returned pointer, to restore global state after processing for this response is completed.
 */
struct ResponseInfo *resume_response_batch(struct ResponseInfo *response);

/* Suspend any currently active response, returning it for later resumption */
static inline struct ResponseInfo *
suspend_response_batch(void)
{
	return resume_response_batch(NULL);
}

/* Get details for a labeled-response batch with the specified batch ID */
struct ResponseInfo *get_remote_response_batch(const char *batch);

/* Free memory and clean up references to a pending labeled-response batch.
 * The second parameter is used to update global state to point away from the freed response;
 * pass the return value of resume_response_batch() if that was called previously, or NULL otherwise.
 */
void free_response_batch(struct ResponseInfo *response, struct ResponseInfo *resume);

/* Count the number of servers matching a given glob mask and capabilities, except for servers in the direction of one
 * This helps provide an accurate count for begin_remote_response_batch() when broadcasting to multiple servers
 */
int count_match_servs(struct Client *one, const char *mask, uint64_t cap, uint64_t negcap);

#endif /* INCLUDED_response_h */
