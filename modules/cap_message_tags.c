/*
 * Solanum: a slightly advanced ircd
 * cap_allow_list.c: implement the message-tags IRCv3 specification
 *
 * Copyright (c) 2022 Ryan Lahfa <ryan@lahfa.xyz>
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
#include "inline/stringops.h"

static const char cap_message_tags_desc[] =
	"Propagate message tags";

static void cap_message_tags_process(void *);
unsigned int CLICAP_MESSAGE_TAGS = 0;

mapi_hfn_list_av1 cap_message_tags_hfnlist[] = {
	{ "outbound_msgbuf", cap_message_tags_process },
	{ NULL, NULL }
};
mapi_cap_list_av2 cap_message_tags_cap_list[] = {
	{ MAPI_CAP_CLIENT, "message-tags", NULL, &CLICAP_MESSAGE_TAGS },
	{ 0, NULL, NULL, NULL },
};

static void
cap_message_tags_process(void *data_)
{
	hook_data_client_tag_accept moduledata;

	hook_data *data = data_;
	struct MsgBuf *msgbuf = data->arg1;

	moduledata.client = data->client;
	moduledata.outgoing_msgbuf = msgbuf;

	if (incoming_client != NULL) {
		size_t n_tags = incoming_message->n_tags;
		for (size_t index = 0 ; index < n_tags ; index++) {
			if (incoming_message->tags[index].key[0] == '+') {
				moduledata.incoming_tag = &incoming_message->tags[index];
				call_hook(h_client_tag_accept, &moduledata);

				// In case a downstream module decides this client-tag
				// warrants a silent drop of the response
				// We honor it
				// e.g. typing notifications spamming
				if (moduledata.drop)
				{
					//data->drop = true;
					//return;
				}
			}
		}
	}
}

DECLARE_MODULE_AV2(cap_message_tags, NULL, NULL, NULL, NULL, cap_message_tags_hfnlist, cap_message_tags_cap_list, NULL, cap_message_tags_desc);
