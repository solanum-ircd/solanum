/*
 * Solanum: a slightly advanced ircd
 * batch.h: support functions for batches
 *
 * Copyright (c) 2025 Ryan Schmidt <skizzerz@skizzerz.net>
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

#ifndef INCLUDED_batch_h
#define INCLUDED_batch_h

#include "msgbuf.h"

#define BATCH_EXPIRY (15)
#define BATCH_ID_LEN (16)

/* If set, nested batches are not automatically dispatched to their handler.
 * Instead, the batch handler must manually process child batches.
 * Child batches are still automatically freed at the end of the handler invocation.
 */
#define BATCH_FLAG_SKIP_CHILDREN 0x01
/* If set, allows all batch types to be nested under this batch.
 * The allowed_children field is ignored and may be set to NULL. */
#define BATCH_FLAG_ALLOW_ALL 0x02

struct Batch
{
	/* server-generated batch ID (15 random characters + trailing null byte) */
	char id[BATCH_ID_LEN];
	/* BATCH command that started this batch */
	const struct MsgBuf *start;
	/* client-generated batch reference tag */
	const char *tag;
	/* batch type */
	const char *type;
	/* time when this batch expires (times out) */
	time_t expires;
	/* For nested batches, points at the batch immediately encapsulating this one
	 * NULL if this batch is not nested
	 */
	struct Batch *parent;
	/* All finished batches nested under this one (linked list of struct Batch *) */
	rb_dlink_list children;
	/* Number of messages in this batch (including the start message) */
	unsigned int len;
	/* A linked list of struct BatchMessage * for each message in the batch */
	rb_dlink_list messages;
};

struct BatchMessage
{
	/* A copy of all string data for the message */
	char *data;
	/* Size of data in bytes */
	size_t datalen;
	/* A MsgBuf whose internal pointers all point to portions of the data array */
	struct MsgBuf msg;
};

struct Client;
typedef void (*batch_cb)(struct Client *client_p, struct Client *source_p, struct Batch *batch, void *userdata);

struct BatchHandler
{
	/* function called when the batch is completed */
	batch_cb handler;
	/* optional data pointer to pass into the handler */
	void *userdata;
	/* bitfield of flags for this handler from the BATCH_FLAG_* constants */
	unsigned int flags;
	/* batch type names that are allowed to be nested under this one (array of strings)
	 * terminate the array with a NULL entry, e.g. { "foo", "bar", "baz", NULL }
	 */
	const char **allowed_children;
};

bool register_batch_handler(const char *type, const struct BatchHandler *handler);
const struct BatchHandler *get_batch_handler(const char *type);
void remove_batch_handler(const char *type);
void generate_batch_id(char *buf, size_t size);
struct Batch *batch_init(struct MsgBuf *start);
void batch_add_msgbuf(struct Batch *batch, struct MsgBuf *msg);
void batch_free(struct Batch *batch);

#endif /* INCLUDED_batch_h */
