/*
 * modules/um_regonlymsg.c
 * Copyright (c) 2020 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include "hash.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"
#include "privilege.h"
#include "s_newconf.h"
#include "logger.h"

static int
um_regonlymsg_modinit(void)
{
	user_modes['R'] = find_umode_slot();
	construct_umodebuf();

	if (!user_modes['R'])
	{
		ierror("um_regonlymsg: unable to allocate usermode slot for +R, unloading module");
		return -1;
	}

	return 0;
}

static void
um_regonlymsg_moddeinit(void)
{
	user_modes['R'] = 0;
	construct_umodebuf();
}

#define IsSetRegOnlyMsg(c)	((c->umodes & user_modes['R']) == user_modes['R'])

static const char um_regonlymsg_desc[] =
	"Provides usermode +R which restricts messages from unregistered users.";

static bool
allow_message(struct Client *source_p, struct Client *target_p)
{
	if (!MyClient(target_p))
		return true;

	if (!IsSetRegOnlyMsg(target_p))
		return true;

	if (!IsPerson(source_p))
		return true;

	/* XXX: controversial?  allow opers to send through +R */
	if (IsOper(source_p))
		return true;

	if (accept_message(source_p, target_p))
		return true;

	if (source_p->user->suser[0])
		return true;

	return false;
}

static bool
add_callerid_accept_for_source(enum message_type msgtype, struct Client *source_p, struct Client *target_p)
{
	if (!MyClient(source_p))
		return true;

	if(msgtype != MESSAGE_TYPE_NOTICE &&
		IsSetRegOnlyMsg(source_p) &&
		!accept_message(target_p, source_p) &&
		!IsOperGeneral(target_p))
	{
		if(rb_dlink_list_length(&source_p->localClient->allow_list) <
				(unsigned long)ConfigFileEntry.max_accept)
		{
			rb_dlinkAddAlloc(target_p, &source_p->localClient->allow_list);
			rb_dlinkAddAlloc(source_p, &target_p->on_allow_list);
		}
		else
		{
			sendto_one_numeric(source_p, ERR_OWNMODE,
					form_str(ERR_OWNMODE),
					target_p->name, "+R");
			return false;
		}
	}

	return true;
}

static void
h_hdl_invite(void *vdata)
{
	hook_data_channel_approval *data = vdata;
	struct Client *source_p = data->client;
	struct Client *target_p = data->target;
	static char errorbuf[BUFSIZE];

	if (data->approved)
		return;

	if (!add_callerid_accept_for_source(MESSAGE_TYPE_PRIVMSG, source_p, target_p))
	{
		data->approved = ERR_NONONREG;
		return;
	}

	if (allow_message(source_p, target_p))
		return;

	snprintf(errorbuf, sizeof errorbuf, form_str(ERR_NONONREG),
		 target_p->name);

	data->approved = ERR_NONONREG;
	data->error = errorbuf;
}

static void
h_hdl_privmsg_user(void *vdata)
{
	hook_data_privmsg_user *data = vdata;
	struct Client *source_p = data->source_p;
	struct Client *target_p = data->target_p;

	if (data->approved)
		return;

	if (!add_callerid_accept_for_source(data->msgtype, source_p, target_p))
	{
		data->approved = ERR_NONONREG;
		return;
	}

	if (allow_message(source_p, target_p))
		return;

	data->approved = ERR_NONONREG;

	if (data->msgtype == MESSAGE_TYPE_NOTICE)
		return;

	sendto_one_numeric(source_p, ERR_NONONREG, form_str(ERR_NONONREG),
			   target_p->name);
}

static mapi_hfn_list_av1 um_regonlymsg_hfnlist[] = {
	{ "invite", h_hdl_invite },
	{ "privmsg_user", h_hdl_privmsg_user },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(um_regonlymsg, um_regonlymsg_modinit, um_regonlymsg_moddeinit,
		   NULL, NULL, um_regonlymsg_hfnlist, NULL, NULL, um_regonlymsg_desc);
