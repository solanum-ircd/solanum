/*
 * Solanum: a slightly advanced ircd
 * tag_message_id.c: implement the message-ids IRCv3 specification
 *
 * Copyright (c) 2025 Ryan Schmidt <skizzerz@skizzerz.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"
#include "chmode.h"
#include "parse.h"
#include "inline/stringops.h"

static const char tag_message_id_desc[] = "Provides the msgid tag";
static void tag_message_id_incoming(void *);
static void add_message_id_user(void *);
static void add_message_id_channel(void *);

mapi_hfn_list_av1 tag_message_id_hfnlist[] = {
	{ "message_tag", tag_message_id_incoming },
	{ "privmsg_user", add_message_id_user },
	{ "privmsg_channel", add_message_id_channel },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(tag_message_id, NULL, NULL, NULL, NULL, tag_message_id_hfnlist, NULL, NULL, tag_message_id_desc);

static void generate_msgid(char *buf, size_t len, struct Client *source_p, const char *target);

static void
tag_message_id_incoming(void *data_)
{
	hook_data_message_tag *data = data_;
	if (IsServer(data->client) && !strcmp("msgid", data->key)) {
		data->capmask = CLICAP_MESSAGE_TAGS;
		data->approved = MESSAGE_TAG_ALLOW;
	}
}

static void
add_message_id_user(void *data_)
{
	static char buf[BUFSIZE];
	hook_data_privmsg_user *data = data_;

	if (msgbuf_get_tag(data->msgbuf, "msgid"))
		return;

	/* don't add msgid to messages received from remote servers to avoid desynced ids across servers */
	if (data->source_p == NULL || !MyClient(data->source_p))
		return;

	generate_msgid(buf, sizeof(buf), data->source_p, NULL);
	msgbuf_append_tag(data->msgbuf, "msgid", buf, CLICAP_MESSAGE_TAGS);
}

static void
add_message_id_channel(void *data_)
{
	static char buf[BUFSIZE];
	hook_data_privmsg_channel *data = data_;

	/* don't give msgid to PARTs */
	if (data->msgtype == MESSAGE_TYPE_PART)
		return;

	if (msgbuf_get_tag(data->msgbuf, "msgid"))
		return;

	/* don't add msgid to messages received from remote servers to avoid desynced ids across servers */
	if (data->source_p == NULL || !MyClient(data->source_p))
		return;

	generate_msgid(buf, sizeof(buf), data->source_p, data->chptr->chname);
	msgbuf_append_tag(data->msgbuf, "msgid", buf, CLICAP_MESSAGE_TAGS);
}

static void
generate_msgid(char *buf, size_t len, struct Client *source_p, const char *target)
{
	static time_t prev_ts = 0;
	static unsigned short prev_ms = 0;
	static unsigned short ctr;

	const struct timeval *tv = rb_current_time_tv();
	time_t ts = tv->tv_sec;
	unsigned short ms = tv->tv_usec / 1000;

	/* (re-)initialize counter if the timestamp changed so it is less useful
		 * to determine exactly how many messages are sent over time */
	if (ts > prev_ts)
	{
		prev_ts = ts;
		prev_ms = ms;
		rb_get_random(&ctr, sizeof(ctr));
		/* clear top bit to allow for overflow */
		ctr &= 0x7fff;
	} else if (ms > prev_ms)
		prev_ms = ms;

	/* handle unlikely overflow case to keep message ids in correctly-sortable order and to avoid dupes */
	if (++ctr == 0)
	{
		prev_ms++;
		if (prev_ms == 1000)
		{
			prev_ts++;
			prev_ms = 0;
		}
	}

	/* Format version 1 contains the following (in order):
	 * 1. The character '1'
	 * 2. Current seconds since epoch (10 characters)
	 * 3. Current milliseconds value for current time (3 characters)
	 * 4. Counter value (6 characters)
	 * 5. Client UID (9 characters)
	 * 6. Base64-encoded channel name if target is a channel (variable number of characters),
	 *    empty string if target is not a channel.
	 */
	char *encoded = NULL;
	if (target != NULL)
		encoded = rb_base64_encode(target, strlen(target));

	snprintf(buf, len, "1%010d%03d%06d%s%s",
		(unsigned)prev_ts, prev_ms, ctr, source_p->id, encoded == NULL ? "" : encoded);

	if (encoded != NULL)
		rb_free(encoded);
}
