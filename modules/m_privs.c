/*
 * m_privs.c: Shows effective operator privileges
 *
 * Copyright (C) 2008 Jilles Tjoelker
 * Copyright (C) 2008 charybdis development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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
#include "client.h"
#include "numeric.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "hash.h"

static const char privs_desc[] = "Provides the PRIVS command to inspect an operator's privileges";

static void m_privs(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static void me_privs(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static void mo_privs(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

struct Message privs_msgtab = {
	"PRIVS", 0, 0, 0, 0,
	{mg_unreg, {m_privs, 0}, mg_ignore, mg_ignore, {me_privs, 0}, {mo_privs, 0}}
};

mapi_clist_av1 privs_clist[] = {
	&privs_msgtab,
	NULL
};

/* XXX this is a copy, not so nice
 *
 * Sort of... it's int in newconf.c since oper confs don't need 64-bit wide flags.
 * --Elizafox
 */
struct mode_table
{
	const char *name;
	uint64_t mode;
};

/* there is no such table like this anywhere else */
static struct mode_table auth_client_table[] = {
	{"resv_exempt",		FLAGS_EXEMPTRESV	},
	{"kline_exempt",	FLAGS_EXEMPTKLINE	},
	{"flood_exempt",	FLAGS_EXEMPTFLOOD	},
	{"spambot_exempt",	FLAGS_EXEMPTSPAMBOT	},
	{"shide_exempt",	FLAGS_EXEMPTSHIDE	},
	{"jupe_exempt",		FLAGS_EXEMPTJUPE	},
	{"extend_chans",	FLAGS_EXTENDCHANS	},
	{NULL, 0}
};

DECLARE_MODULE_AV2(privs, NULL, NULL, privs_clist, NULL, NULL, NULL, NULL, privs_desc);

static void show_privs(struct Client *source_p, struct Client *target_p)
{
	struct mode_table *p;

	send_multiline_init(source_p, " ", form_str(RPL_PRIVS),
			get_id(&me, source_p),
			get_id(source_p, source_p),
			target_p->name,
			"* ");

	send_multiline_remote_pad(source_p, &me);
	send_multiline_remote_pad(source_p, source_p);

	if (target_p->user->privset)
		for (const char *const *s = privilegeset_privs(target_p->user->privset); *s != NULL; s++)
			send_multiline_item(source_p, "%s", *s);

	if (IsOper(target_p))
	{
		if (target_p->user->opername)
			send_multiline_item(source_p, "operator:%s", target_p->user->opername);

		if (target_p->user->privset)
			send_multiline_item(source_p, "privset:%s", target_p->user->privset->name);
	}
	p = &auth_client_table[0];
	while (p->name != NULL)
	{
		if (target_p->flags & p->mode)
			send_multiline_item(source_p, "%s", p->name);
		p++;
	}

	send_multiline_fini(source_p, form_str(RPL_PRIVS),
			get_id(&me, source_p),
			get_id(source_p, source_p),
			target_p->name,
			"");
}

static void
me_privs(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;

	if (!IsOper(source_p) || parc < 2 || EmptyString(parv[1]))
		return;

	target_p = find_person(parv[1]);

	if (target_p != NULL)
		show_privs(source_p, target_p);
}

static void
mo_privs(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p;
	struct Client *server_p;

	if (parc < 2 || EmptyString(parv[1]))
	{
		server_p = target_p = source_p;
	}
	else
	{
		if (parc >= 3)
		{
			server_p = find_named_client(parv[1]);
			target_p = find_named_person(parv[2]);
		}
		else
		{
			server_p = target_p = find_named_person(parv[1]);
		}
		if (server_p == NULL || target_p == NULL)
		{
			sendto_one_numeric(source_p, ERR_NOSUCHNICK,
					   form_str(ERR_NOSUCHNICK), parv[1]);
			return;
		}
	}

	if (target_p != source_p && !HasPrivilege(source_p, "oper:privs"))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "privs");
		return;
	}

	if (!IsServer(server_p))
		server_p = server_p->servptr;

	if (IsMe(server_p))
		show_privs(source_p, target_p);
	else
		sendto_one(server_p, ":%s ENCAP %s PRIVS %s",
				get_id(source_p, server_p),
				server_p->name,
				use_id(target_p));
}

static void
m_privs(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if (parc >= 2 && !EmptyString(parv[1]) &&
			irccmp(parv[1], source_p->name)) {
		sendto_one_numeric(source_p, ERR_NOPRIVILEGES,
				   form_str(ERR_NOPRIVILEGES));
		return;
	}

	show_privs(source_p, source_p);
}
