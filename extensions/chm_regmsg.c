/*
 * Solanum: a slightly advanced ircd
 * chm_regmsg: require identification to chat (+R mode).
 *
 * Copyright (c) 2020 Eric Mertens <emertens@gmail.com>
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
#include "logger.h"
#include "send.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"
#include "chmode.h"
#include "inline/stringops.h"

static const char chm_regmsg_desc[] =
	"Adds channel mode +R, which blocks messagess from unregistered users";

static unsigned int mode_regmsg;

static void chm_regmsg_process(void *);

mapi_hfn_list_av1 chm_regmsg_hfnlist[] = {
	{ "privmsg_channel", chm_regmsg_process },
	{ NULL, NULL }
};

static void
chm_regmsg_process(void *data_)
{
	hook_data_privmsg_channel *data = data_;
	struct membership *msptr;

	/* message is already blocked, defer */
	if (data->approved)
		return;

	/* user is identified, accept */
	if (!EmptyString(data->source_p->user->suser))
		return;

	/* voice and op override identification requirement, accept */
	msptr = find_channel_membership(data->chptr, data->source_p);
	if (is_chanop_voiced(msptr))
		return;

	sendto_one_numeric(data->source_p, ERR_MSGNEEDREGGEDNICK, form_str(ERR_MSGNEEDREGGEDNICK),
		data->source_p->name, data->chptr->chname);
	data->approved = ERR_MSGNEEDREGGEDNICK;
}

static int
_modinit(void)
{
	mode_regmsg = cflag_add('R', chm_simple);
	if (mode_regmsg == 0) {
		ierror("chm_regmsg: unable to allocate cmode slot for +R, unloading module");
		return -1;
	}

	return 0;
}

static void
_moddeinit(void)
{
	cflag_orphan('R');
}

DECLARE_MODULE_AV2(chm_regmsg, _modinit, _moddeinit, NULL, NULL, chm_regmsg_hfnlist, NULL, NULL, chm_regmsg_desc);
