/*
 * Solanum: a slightly advanced ircd
 * cap_server_time.c: implement the server-time IRCv3.2 capability
 *
 * Copyright (c) 2016 Ariadne Conill <ariadne@dereferenced.org>
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

static const char cap_server_time_desc[] =
	"Provides the server-time client capability";

static void cap_server_time_process(hook_data *);
unsigned int CLICAP_SERVER_TIME = 0;

mapi_hfn_list_av1 cap_server_time_hfnlist[] = {
	{ "outbound_msgbuf", (hookfn) cap_server_time_process },
	{ NULL, NULL }
};
mapi_cap_list_av2 cap_server_time_cap_list[] = {
	{ MAPI_CAP_CLIENT, "server-time", NULL, &CLICAP_SERVER_TIME },
	{ 0, NULL, NULL, NULL }
};

static void
cap_server_time_process(hook_data *data)
{
	static char buf[BUFSIZE];
	struct MsgBuf *msgbuf = data->arg1;
	struct timeval tv;

	if (!rb_gettimeofday(&tv, NULL)) {
		if (strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S.", gmtime(&tv.tv_sec)) == 0)
			return;

		if (rb_snprintf_append(buf, sizeof(buf), "%03uZ", (int)tv.tv_usec / 1000) < 0)
			return;

		msgbuf_append_tag(msgbuf, "time", buf, CLICAP_SERVER_TIME);
	}
}

DECLARE_MODULE_AV2(cap_server_time, NULL, NULL, NULL, NULL, cap_server_time_hfnlist, cap_server_time_cap_list, NULL, cap_server_time_desc);
