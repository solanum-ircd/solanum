/*
 * Solanum: a slightly advanced ircd
 * m_batch.c: provides support for client-initiated and propagated BATCH commands
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
#include "client.h"
#include "hash.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "hook.h"
#include "modules.h"
#include "batch.h"

static const char batch_desc[] =
	"Provides the BATCH command for client-initiated or propagated batches";

static struct ev_entry *timeout_ev;

static int batch_modinit(void);
static void batch_moddeinit(void);
static void process_batch_tag(void *);
static void handle_batch_line(void *);
static void handle_client_exit(void *);
static void m_queue(struct MsgBuf *msgbuf, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static void m_batch(struct MsgBuf *msgbuf, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

static struct Message batch_msgtab = {
	"BATCH", 0, 0, 0, 0,
	{mg_unreg, {m_batch, 2}, {m_batch, 2}, {m_batch, 2}, mg_ignore, {m_batch, 2}}
};

mapi_clist_av1 batch_clist[] = { &batch_msgtab, NULL };

mapi_hfn_list_av1 batch_hfnlist[] = {
	{ "message_handler", handle_batch_line },
	{ "message_tag", process_batch_tag },
	{ "client_exit", handle_client_exit },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(batch, batch_modinit, batch_moddeinit, batch_clist, NULL, batch_hfnlist, NULL, NULL, batch_desc);

static void
batch_timeout(void *arg)
{
	/* non-NULL arg indicates this is being called during module unload so we need to "time out" all batches */
	rb_dlink_node *cptr, *ptr, *next_ptr;
	struct Client *client;
	struct Batch *batch;
	time_t now = rb_current_time();

	RB_DLINK_FOREACH(cptr, lclient_list.head)
	{
		client = cptr->data;
		RB_DLINK_FOREACH_SAFE(ptr, next_ptr, client->localClient->pending_batches.head)
		{
			batch = ptr->data;
			if (arg != NULL || batch->expires <= now)
			{
				sendto_one(client, ":%s FAIL BATCH TIMEOUT %s :Batch timed out",
					me.name, batch->tag);
				client->localClient->pending_batch_lines -= batch->len;
				batch_free(batch);
				rb_dlinkDestroy(ptr, &client->localClient->pending_batches);
			}
		}
	}

	if (arg != NULL)
	{
		/* get rid of server batches only on module unload */
		RB_DLINK_FOREACH(cptr, serv_list.head)
		{
			client = cptr->data;
			RB_DLINK_FOREACH_SAFE(ptr, next_ptr, client->localClient->pending_batches.head)
			{
				batch_free(ptr->data);
				rb_dlinkDestroy(ptr, &client->localClient->pending_batches);
			}

			client->localClient->pending_batch_lines = 0;
		}
	}
}

static void
handle_client_exit(void *data_)
{
	hook_data_client_exit *data = data_;
	rb_dlink_node *ptr, *next_ptr;

	if (!MyConnect(data->target))
		return;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, data->target->localClient->pending_batches.head)
	{
		batch_free(ptr->data);
		rb_dlinkDestroy(ptr, &data->target->localClient->pending_batches);
	}

	/* probably unnecessary but makes me feel better */
	data->target->localClient->pending_batch_lines = 0;
}

static void
finish_batch(struct Client *client_p, struct Client *source_p, struct Batch *batch)
{
	const struct BatchHandler *handler = get_batch_handler(batch->type);
	rb_dlink_node *ptr, *next_ptr;

	/* abort unfinished nested batches under this one */
	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, client_p->localClient->pending_batches.head)
	{
		struct Batch *other = ptr->data;
		if (other->parent == batch)
		{
			if (IsClient(source_p))
				sendto_one(source_p, ":%s FAIL BATCH INCOMPLETE %s :Nested batch not finished before enclosing batch",
					me.name, other->tag);

			client_p->localClient->pending_batch_lines -= other->len;
			batch_free(other);
			rb_dlinkDestroy(ptr, &client_p->localClient->pending_batches);
		}
	}

	client_p->localClient->pending_batch_lines -= batch->len;

	if (handler == NULL)
	{
		/* getting here means that we originally accepted this batch type but no longer recognize it;
		 * e.g. the supporting module got unloaded between batch start and finish.
		 * There isn't a good solution here so just reject it and let the client send any fallbacks separately. */
		if (IsClient(source_p))
			sendto_one(source_p, ":%s FAIL BATCH UNKNOWN_TYPE %s %s :Unrecognized batch type",
				me.name, batch->tag, batch->type);

		batch_free(batch);
		return;
	}

	bool skip_children = (handler->flags & BATCH_FLAG_SKIP_CHILDREN) == BATCH_FLAG_SKIP_CHILDREN;

	/* don't trigger handlers for empty batches; treat as no-ops instead */
	if (batch->len > 1 || (skip_children && batch->children.length > 0))
	{
		handler->handler(client_p, source_p, batch, handler->userdata);
	}

	/* handle child batches */
	if (!skip_children)
	{
		RB_DLINK_FOREACH_SAFE(ptr, next_ptr, batch->children.head)
		{
			struct Batch *child = ptr->data;
			rb_dlinkDestroy(ptr, &batch->children);
			finish_batch(client_p, source_p, child);
		}
	}

	batch_free(batch);
}

static void
handle_batch_line(void *data_)
{
	hook_data *data = data_;
	struct MsgBuf *msgbuf = data->arg1;
	struct MessageEntry *ehandler = data->arg2;

	if (msgbuf_get_tag(msgbuf, "batch") != NULL && strcasecmp(msgbuf->cmd, "BATCH") != 0)
		ehandler->handler = m_queue;
}

static void
m_queue(struct MsgBuf *msgbuf, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	rb_dlink_node *ptr;
	const char *tag = msgbuf_get_tag(msgbuf, "batch");

	if (EmptyString(tag))
		return;

	RB_DLINK_FOREACH(ptr, client_p->localClient->pending_batches.head)
	{
		struct Batch *batch = ptr->data;
		if (!strcmp(batch->tag, tag))
		{
			batch_add_msgbuf(batch, msgbuf);
			client_p->localClient->pending_batch_lines++;
			return;
		}
	}
}

static void
m_batch(struct MsgBuf *msgbuf, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Batch *batch = NULL, *parent = NULL;
	rb_dlink_node *ptr;
	bool found = false;
	bool adding = *parv[1] == '+';
	const char *batch_tag = msgbuf_get_tag(msgbuf, "batch");

	if (batch_tag != NULL)
	{
		bool found_parent = false;
		RB_DLINK_FOREACH(ptr, client_p->localClient->pending_batches.head)
		{
			parent = ptr->data;
			if (!strcmp(parent->tag, batch_tag))
			{
				found_parent = true;
				break;
			}
		}

		if (!found_parent)
			return;
	}

	if (EmptyString(parv[1]) || parc < (adding ? 3 : 2))
	{
		if (IsClient(source_p))
			sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "BATCH");
		return;
	}

	if (adding && EmptyString(parv[2]))
	{
		if (IsClient(source_p))
			sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "BATCH");
		return;
	}

	if ((!adding && *parv[1] != '-') || EmptyString(parv[1] + 1))
	{
		if (IsClient(source_p))
			sendto_one(source_p, ":%s FAIL BATCH INVALID_REFTAG %s :Invalid reference tag",
				me.name, parv[1]);
		return;
	}

	RB_DLINK_FOREACH(ptr, client_p->localClient->pending_batches.head)
	{
		batch = ptr->data;
		if (!strcmp(batch->tag, parv[1] + 1))
		{
			found = true;
			break;
		}
	}

	if (adding)
	{
		if (found)
		{
			if (IsClient(source_p))
				sendto_one(source_p, ":%s FAIL BATCH INVALID_REFTAG %s :Reference tag already exists",
					me.name, parv[1]);
			return;
		}

		if (get_batch_handler(parv[2]) == NULL)
		{
			if (IsClient(source_p))
				sendto_one(source_p, ":%s FAIL BATCH UNKNOWN_TYPE %s %s :Unrecognized batch type",
					me.name, parv[1], parv[2]);
			return;
		}

		if (parent != NULL)
		{
			const struct BatchHandler *parent_handler = get_batch_handler(parent->type);
			if (parent_handler == NULL)
			{
				/* parent batch type got unregistered while we were in the middle of receiving this batch... */
				if (IsClient(source_p))
					sendto_one(source_p, ":%s FAIL BATCH UNKNOWN_TYPE %s %s :Unrecognized batch type",
						me.name, parent->tag, parent->type);
				return;
			}

			bool allowed = (parent_handler->flags & BATCH_FLAG_ALLOW_ALL) == BATCH_FLAG_ALLOW_ALL;
			if (!allowed && parent_handler->allowed_children != NULL)
			{
				for (int i = 0; parent_handler->allowed_children[i] != NULL; ++i)
				{
					if (!strcmp(parv[2], parent_handler->allowed_children[i]))
					{
						allowed = true;
						break;
					}
				}
			}

			if (!allowed)
			{
				if (IsClient(source_p))
					sendto_one(source_p, ":%s FAIL BATCH INVALID_NESTING %s %s %s :The parent batch type does not allow this type to be nested under it",
						me.name, parv[1], parent->type, parv[2]);
				return;
			}
		}

		batch = batch_init(msgbuf);
		batch->parent = parent;
		rb_dlinkAddAlloc(batch, &client_p->localClient->pending_batches);
		client_p->localClient->pending_batch_lines++;
	}
	else
	{
		if (!found)
		{
			if (IsClient(source_p))
				sendto_one(source_p, ":%s FAIL BATCH INVALID_REFTAG %s :Invalid reference tag",
					me.name, parv[1]);
			return;
		}

		rb_dlinkDestroy(ptr, &client_p->localClient->pending_batches);

		if (batch->parent != NULL)
		{
			/* Finished a nested batch but the outer batch isn't complete yet, don't process just yet
			 * Instead, add this batch to the parent so it gets completed alongside the parent
			 */
			rb_dlinkAddTailAlloc(batch, &batch->parent->children);
			/* we deducted this earlier but the batch is still pending, so re-add the consumed lines */
			client_p->localClient->pending_batch_lines += batch->len;
		}
		else
		{
			finish_batch(client_p, source_p, batch);
		}
	}
}

static void
process_batch_tag(void *data_)
{
	hook_data_message_tag *data = data_;

	if (!strcmp(data->key, "batch"))
	{
		if (IsServer(data->client) || !EmptyString(data->value))
		{
			data->approved = MESSAGE_TAG_ALLOW;
			data->capmask = CLICAP_BATCH;
		}
	}
}

static int
batch_modinit(void)
{
	timeout_ev = rb_event_addish("batch-timeout", &batch_timeout, NULL, 30);
	return 1;
}

static void
batch_moddeinit(void)
{
	batch_timeout((void *)1);
	rb_event_delete(timeout_ev);
}
