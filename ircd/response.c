/*
 * Solanum: a slightly advanced ircd
 * response.c: labeled-response helpers
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

#include "stdinc.h"
#include "rb_dictionary.h"
#include "batch.h"
#include "client.h"
#include "response.h"
#include "send.h"
#include "s_serv.h"

/* number of seconds we'll wait for remote servers to finish sending their responses
 * before we abort a pending remote labeled-response batch
 */
#define RESPONSE_EXPIRY 10

struct ResponseInfo *outgoing_response_info;
static rb_dictionary *pending_responses;

static int
response_cmp(const void *s1, const void *s2)
{
	return strcmp(s1, s2);
}

static void
cleanup_pending_responses(void *unused)
{
	struct ResponseInfo *response;
	rb_dictionary_iter iter;
	rb_dlink_list freelist = { NULL, NULL, 0 };
	rb_dlink_node *ptr, *nptr;
	time_t now = rb_current_time();

	/* RB_DICTIONARY_FOREACH is not safe for deletion, so need to do this in two passes */
	RB_DICTIONARY_FOREACH(response, &iter, pending_responses)
	{
		if (response->remote_response > 0 && response->expires < now)
		{
			rb_dlinkAddAlloc(response, &freelist);
		}
	}

	RB_DLINK_FOREACH_SAFE(ptr, nptr, freelist.head)
	{
		response = ptr->data;
		sendto_one(response->source_p, ":%s BATCH -%s", me.name, response->batch);
		rb_dlinkDestroy(ptr, &freelist);
		rb_dictionary_delete(pending_responses, response->batch);
		free_response_batch(response);
	}
}

void
init_response(void)
{
	pending_responses = rb_dictionary_create("pending remote labeled responses", response_cmp);

	rb_event_addish("cleanup_pending_responses", &cleanup_pending_responses, NULL, 10);
}

void
begin_local_response_batch(void)
{
	if (outgoing_response_info == NULL || !MyConnect(outgoing_response_info->source_p))
		return;

	if (!EmptyString(outgoing_response_info->batch))
		return;

	generate_batch_id(outgoing_response_info->batch, sizeof(outgoing_response_info->batch));
	sendto_one(outgoing_response_info->source_p, ":%s BATCH +%s labeled-response",
		me.name, outgoing_response_info->batch);
}

void
begin_remote_response_batch(int server_count)
{
	if (outgoing_response_info == NULL || !MyConnect(outgoing_response_info->source_p))
		return;

	if (!EmptyString(outgoing_response_info->batch))
		return;

	generate_batch_id(outgoing_response_info->batch, sizeof(outgoing_response_info->batch));
	outgoing_response_info->remote_response = server_count;
	outgoing_response_info->expires = rb_current_time() + RESPONSE_EXPIRY;
	rb_dlinkAddAlloc(outgoing_response_info, &outgoing_response_info->source_p->localClient->pending_remote_responses);
	rb_dictionary_add(pending_responses, outgoing_response_info->batch, outgoing_response_info);
	sendto_one(outgoing_response_info->source_p, ":%s BATCH +%s labeled-response",
		me.name, outgoing_response_info->batch);
}

struct ResponseInfo *
get_remote_response_batch(const char *batch)
{
	return rb_dictionary_retrieve(pending_responses, batch);
}

void
free_response_batch(struct ResponseInfo *response)
{
	rb_dlink_node *ptr, *nptr;

	if (response == NULL)
		return;

	if (MyConnect(response->source_p))
	{
		RB_DLINK_FOREACH_SAFE(ptr, nptr, response->source_p->localClient->pending_remote_responses.head)
		{
			if (ptr->data == response)
			{
				rb_dlinkDestroy(ptr, &response->source_p->localClient->pending_remote_responses);
				break;
			}
		}

		rb_dictionary_delete(pending_responses, response->batch);
	}

	rb_free(response);
}

int
count_match_servs(struct Client *one, const char *mask, uint64_t cap, uint64_t negcap)
{
	int count = 0;
	rb_dlink_node *ptr;

	if (EmptyString(mask))
		return 0;

	RB_DLINK_FOREACH(ptr, global_serv_list.head)
	{
		struct Client *target_p = ptr->data;
		if (target_p == &me)
			continue;

		if (target_p->from == one->from)
			continue;

		if (!match(mask, target_p->name))
			continue;

		if (cap && !IsServerCapable(target_p->from, cap))
			continue;

		if (negcap && !NotServerCapable(target_p->from, negcap))
			continue;

		count++;
	}

	return count;
}
