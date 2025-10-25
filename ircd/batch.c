/*
 * Solanum: a slightly advanced ircd
 * batch.c: support functions for batches
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

#include "stdinc.h"
#include "batch.h"
#include "rb_dictionary.h"

static rb_dictionary *handlers = NULL;

bool
register_batch_handler(const char *type, const struct BatchHandler *handler)
{
	if (handlers == NULL)
		handlers = rb_dictionary_create("batch handlers", rb_strcasecmp);

	if (rb_dictionary_find(handlers, type) != NULL)
		return false;

	rb_dictionary_add(handlers, type, (void *)handler);
	return true;
}

const struct BatchHandler *
get_batch_handler(const char *type)
{
	if (handlers == NULL)
		return NULL;

	rb_dictionary_element *elem = rb_dictionary_find(handlers, type);
	if (elem == NULL)
		return NULL;

	return elem->data;
}

void
remove_batch_handler(const char *type)
{
	if (handlers == NULL)
		return;

	rb_dictionary_delete(handlers, type);
}

void
generate_batch_id(char *buf, size_t size)
{
	static const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-A";

	if (size > BATCH_ID_LEN + 1)
		size = BATCH_ID_LEN + 1;

	for (size_t i = 0; i < size - 1; i++)
		buf[i] = alphabet[rand() % 64];

	buf[size - 1] = 0;
}

struct Batch *
batch_init(struct MsgBuf *start)
{
	struct Batch *batch = rb_malloc(sizeof(struct Batch));
	batch_add_msgbuf(batch, start);

	generate_batch_id(batch->id, sizeof(batch->id));
	batch->start = &((struct BatchMessage *)batch->messages.head->data)->msg;
	batch->tag = batch->start->para[1] + 1;
	batch->type = batch->start->para[2];
	batch->expires = rb_current_time() + BATCH_EXPIRY;

	return batch;
}

void
batch_free(struct Batch *batch)
{
	rb_dlink_node *ptr, *next_ptr;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, batch->messages.head)
	{
		struct BatchMessage *message = ptr->data;
		rb_dlinkDestroy(ptr, &batch->messages);
		rb_free(message->data);
		rb_free(message);
	}

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, batch->children.head)
	{
		struct Batch *child = ptr->data;
		rb_dlinkDestroy(ptr, &batch->children);
		batch_free(child);
	}

	rb_free(batch);
}

void
batch_add_msgbuf(struct Batch *batch, struct MsgBuf *msg)
{
	unsigned int len = 0;
	char *c;
	struct BatchMessage *copy = rb_malloc(sizeof(struct BatchMessage));

	for (int i = 0; i < msg->n_tags; i++)
	{
		len += strlen(msg->tags[i].key) + 1;
		len += strlen(msg->tags[i].value) + 1;
	}

	for (int i = 0; i < msg->n_para; i++)
	{
		len += strlen(msg->para[i]) + 1;
	}

	if (msg->origin != NULL)
		len += strlen(msg->origin) + 1;

	if (msg->cmd != NULL)
		len += strlen(msg->cmd) + 1;

	if (msg->target != NULL)
		len += strlen(msg->target) + 1;

	copy->data = rb_malloc(len);
	copy->datalen = len;
	c = copy->data;

	copy->msg.n_tags = msg->n_tags;
	copy->msg.tagslen = msg->tagslen;
	for (int i = 0; i < msg->n_tags; i++)
	{
		strcpy(c, msg->tags[i].key);
		copy->msg.tags[i].key = c;
		c += strlen(c) + 1;

		strcpy(c, msg->tags[i].value);
		copy->msg.tags[i].value = c;
		c += strlen(c) + 1;
	}

	copy->msg.n_para = msg->n_para;
	for (int i = 0; i < msg->n_para; i++)
	{
		strcpy(c, msg->para[i]);
		copy->msg.para[i] = c;
		c += strlen(c) + 1;
	}

	copy->msg.endp = c - 1;

	if (msg->origin == NULL)
		copy->msg.origin = NULL;
	else
	{
		strcpy(c, msg->origin);
		copy->msg.origin = c;
		c += strlen(c) + 1;
	}

	if (msg->cmd == NULL)
		copy->msg.cmd = NULL;
	else
	{
		strcpy(c, msg->cmd);
		copy->msg.cmd = c;
		c += strlen(c) + 1;
	}

	if (msg->target == NULL)
		copy->msg.target = NULL;
	else
	{
		strcpy(c, msg->target);
		copy->msg.target = c;
	}

	copy->msg.preserve_trailing = msg->preserve_trailing;
	batch->len++;
	rb_dlinkAddTailAlloc(copy, &batch->messages);
}
