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

/* format version for message ids, increment if the representation changes in a meaningful way
 * e.g. if we start encoding information in the ID that we or other servers can later extract */
#define MESSAGE_ID_FORMAT 1

static const char tag_message_id_desc[] = "Provides the msgid tag";
static void tag_message_id_incoming(void *);
static void tag_message_id_outgoing(void *);

mapi_hfn_list_av1 tag_message_id_hfnlist[] = {
	{ "message_tag", tag_message_id_incoming },
	{ "outbound_msgbuf", tag_message_id_outgoing },
	{ NULL, NULL }
};

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
tag_message_id_outgoing(void *data_)
{
	static char buf[BUFSIZE];
	static time_t prev_ts = 0;
	static unsigned short prev_ms = 0;
	static unsigned short ctr;
	const char *incoming_msgid;

	hook_data *data = data_;
	const struct timeval *tv = rb_current_time_tv();
	time_t ts = tv->tv_sec;
	unsigned short ms = tv->tv_usec / 1000;

	struct MsgBuf *msgbuf = data->arg1;

	if (msgbuf_get_tag(msgbuf, "msgid"))
		return;

	if (incoming_client != NULL && IsServer(incoming_client) && (incoming_msgid = msgbuf_get_tag(incoming_message, "msgid")) != NULL)
	{
		msgbuf_append_tag(msgbuf, "msgid", incoming_msgid, CLICAP_MESSAGE_TAGS);
		return;
	}

	if (data->client == NULL || IsMe(data->client) || !MyClient(data->client) || msgbuf->cmd == NULL)
		return;

	if (!strcmp(msgbuf->cmd, "PRIVMSG") || !strcmp(msgbuf->cmd, "NOTICE") || !strcmp(msgbuf->cmd, "TAGMSG"))
	{
		/* Invalid target? */
		if (msgbuf->n_para < 2 || EmptyString(msgbuf->para[1]))
			return;

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
		 *    empty string if target is not a channel. We know this is PRIVMSG, NOTICE, or TAGMSG
		 *    so msgbuf->para[1] is guaranteed to be our target.
		 */
		char *encoded = NULL;
		if (IsChannelName(msgbuf->para[1]) || IsChannelName(msgbuf->para[1] + 1))
			encoded = rb_base64_encode(msgbuf->para[1], strlen(msgbuf->para[1]));

		snprintf(buf, sizeof(buf), "%c%010d%03d%06d%s%s",
			MESSAGE_ID_FORMAT + '0', (unsigned)prev_ts, prev_ms, ctr, data->client->id,
			encoded == NULL ? "" : encoded);
		if (encoded != NULL)
			rb_free(encoded);

		msgbuf_append_tag(msgbuf, "msgid", buf, CLICAP_MESSAGE_TAGS);
	}
}

DECLARE_MODULE_AV2(tag_message_id, NULL, NULL, NULL, NULL, tag_message_id_hfnlist, NULL, NULL, tag_message_id_desc);
