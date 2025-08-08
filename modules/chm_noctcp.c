/*
 * Solanum: a slightly advanced ircd
 * chm_noctcp: block non-action CTCP (+C mode).
 *
 * Copyright (c) 2012 Ariadne Conill <ariadne@dereferenced.org>
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

static const char chm_noctcp_desc[] =
	"Adds channel mode +C, which blocks CTCP messages from a channel (except ACTION)";

static unsigned int mode_noctcp;

static void chm_noctcp_process(void *);

mapi_hfn_list_av1 chm_noctcp_hfnlist[] = {
	{ "privmsg_channel", chm_noctcp_process },
	{ NULL, NULL }
};

static void
chm_noctcp_process(void *data_)
{
	hook_data_privmsg_channel *data = data_;
	/* don't waste CPU if message is already blocked */
	if (data->approved)
		return;

	if (*data->text == '\001' &&
	    data->chptr->mode.mode & mode_noctcp &&
	    !(data->msgtype == MESSAGE_TYPE_PRIVMSG && !rb_strncasecmp(data->text + 1, "ACTION ", 7)))
	{
		sendto_one_numeric(data->source_p, ERR_CANNOTSENDTOCHAN, form_str(ERR_CANNOTSENDTOCHAN), data->chptr->chname);
		data->approved = ERR_CANNOTSENDTOCHAN;
		return;
	}
	else if (rb_dlink_list_length(&data->chptr->locmembers) > (unsigned)(GlobalSetOptions.floodcount / 2))
		data->source_p->large_ctcp_sent = rb_current_time();
}

static int
_modinit(void)
{
	mode_noctcp = cflag_add('C', chm_simple);
	if (mode_noctcp == 0)
		return -1;

	return 0;
}

static void
_moddeinit(void)
{
	cflag_orphan('C');
}

DECLARE_MODULE_AV2(chm_noctcp, _modinit, _moddeinit, NULL, NULL, chm_noctcp_hfnlist, NULL, NULL, chm_noctcp_desc);
