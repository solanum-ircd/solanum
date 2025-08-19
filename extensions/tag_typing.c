/*
 * Solanum: a slightly advanced ircd
 * tag_typing.c: implement the IRCv3 +typing client tag
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
#include "client_tags.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"
#include "chmode.h"
#include "parse.h"
#include "inline/stringops.h"

static const char tag_typing_desc[] = "Provides support for the +typing client tag.";
static void tag_typing_allow(void *);

mapi_hfn_list_av1 tag_typing_hfnlist[] = {
	{ "message_tag", tag_typing_allow },
	{ NULL, NULL }
};

static int
modinit(void)
{
	add_client_tag("typing");
	return 0;
}

static void
moddeinit(void)
{
	remove_client_tag("typing");
}

static void
tag_typing_allow(void *data_)
{
	hook_data_message_tag *data = data_;
	if (!strcmp("+typing", data->key) && data->value != NULL && (!strcmp("active", data->value) || !strcmp("paused", data->value) || !strcmp("done", data->value))) {
		data->capmask = CLICAP_MESSAGE_TAGS;
		data->approved = MESSAGE_TAG_ALLOW;
	}
}

DECLARE_MODULE_AV2(tag_message_id, modinit, moddeinit, NULL, NULL, tag_typing_hfnlist, NULL, NULL, tag_typing_desc);
