/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_webirc.c: Makes CGI:IRC users appear as coming from their real host
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2006 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */
/* Usage:
 * auth {
 *   user = "webirc@<cgiirc ip>"; # if identd used, put ident username instead
 *   password = "<password>"; # encryption possible
 *   spoof = "webirc."
 *   class = "users";
 * };
 * Possible flags:
 *   encrypted - password is encrypted (recommended)
 *   kline_exempt - klines on the cgiirc ip are ignored
 * dlines are checked on the cgiirc ip (of course).
 * k/d/x lines, auth blocks, user limits, etc are checked using the
 * real host/ip.
 * The password should be specified unencrypted in webirc_password in
 * cgiirc.config
 */

#include "stdinc.h"
#include "client.h"		/* client struct */
#include "match.h"
#include "hostmask.h"
#include "send.h"		/* sendto_one */
#include "numeric.h"		/* ERR_xxx */
#include "ircd.h"		/* me */
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_serv.h"
#include "hash.h"
#include "s_conf.h"
#include "reject.h"

static const char webirc_desc[] = "Adds support for the WebIRC system";

static void mr_webirc(struct MsgBuf *msgbuf_p, struct Client *, struct Client *, int, const char **);

struct Message webirc_msgtab = {
	"WEBIRC", 0, 0, 0, 0,
	{{mr_webirc, 5}, mg_reg, mg_ignore, mg_ignore, mg_ignore, mg_reg}
};

mapi_clist_av1 webirc_clist[] = { &webirc_msgtab, NULL };

static void new_local_user(void *data);
mapi_hfn_list_av1 webirc_hfnlist[] = {
	/* unintuitive but correct--we want to be called first */
	{ "new_local_user", new_local_user, HOOK_LOWEST },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(webirc, NULL, NULL, webirc_clist, NULL, webirc_hfnlist, NULL, NULL, webirc_desc);

/*
 * mr_webirc - webirc message handler
 *	parv[1] = password
 *	parv[2] = fake username
 *	parv[3] = fake hostname
 *	parv[4] = fake ip
 */
static void
mr_webirc(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct ConfItem *aconf;
	const char *encr;
	struct rb_sockaddr_storage addr;

	int secure = 0;

	if (source_p->flags & FLAGS_SENTUSER || !EmptyString(source_p->name))
	{
		exit_client(client_p, source_p, &me, "WEBIRC may not follow NICK/USER");
	}

	aconf = find_address_conf(client_p->host, client_p->sockhost,
				IsGotId(client_p) ? client_p->username : "webirc",
				IsGotId(client_p) ? client_p->username : "webirc",
				(struct sockaddr *) &client_p->localClient->ip,
				GET_SS_FAMILY(&client_p->localClient->ip), NULL);

	if (aconf == NULL || !(aconf->status & CONF_CLIENT))
		return;

	if (!IsConfDoSpoofIp(aconf) || irccmp(aconf->info.name, "webirc."))
	{
		/* XXX */
		exit_client(client_p, source_p, &me, "Not a CGI:IRC auth block");
		return;
	}
	if (EmptyString(aconf->passwd))
	{
		exit_client(client_p, source_p, &me, "CGI:IRC auth blocks must have a password");
		return;
	}
	if (!IsSecure(source_p) && aconf->flags & CONF_FLAGS_NEED_SSL)
	{
		exit_client(client_p, source_p, &me, "Your CGI:IRC block requires TLS");
		return;
	}

	if (EmptyString(parv[1]))
		encr = "";
	else if (IsConfEncrypted(aconf))
		encr = rb_crypt(parv[1], aconf->passwd);
	else
		encr = parv[1];

	if (encr == NULL || strcmp(encr, aconf->passwd))
	{
		exit_client(client_p, source_p, &me, "CGI:IRC password incorrect");
		return;
	}

	if (rb_inet_pton_sock(parv[4], &addr) <= 0)
	{
		exit_client(client_p, source_p, &me, "Invalid IP");
		return;
	}

	source_p->localClient->ip = addr;
	source_p->username[0] = '\0';
	ClearGotId(source_p);

	if (parc >= 6)
	{
		const char *s;
		for (s = parv[5]; s != NULL; (s = strchr(s, ' ')) && s++)
		{
			if (!ircncmp(s, "secure", 6) && (s[6] == '=' || s[6] == ' ' || s[6] == '\0'))
				secure = 1;
		}
	}

	if (secure && !IsSecure(source_p))
	{
		sendto_one(source_p, "NOTICE * :CGI:IRC is not connected securely; marking you as insecure");
		secure = 0;
	}

	if (!secure)
	{
		ClearSecure(source_p);
	}

	rb_inet_ntop_sock((struct sockaddr *)&source_p->localClient->ip, source_p->sockhost, sizeof(source_p->sockhost));

	if(strlen(parv[3]) <= HOSTLEN)
		rb_strlcpy(source_p->host, parv[3], sizeof(source_p->host));
	else
		rb_strlcpy(source_p->host, source_p->sockhost, sizeof(source_p->host));

	if (aconf->flags & CONF_FLAGS_SPOOF_IDENT)
	{
		if (parv[2][0] != '~')
			SetGotId(source_p);
		rb_strlcpy(source_p->username, parv[2], sizeof(source_p->username));
	}

	/* Check dlines now, klines will be checked on registration */
	if((aconf = find_dline((struct sockaddr *)&source_p->localClient->ip,
			       GET_SS_FAMILY(&source_p->localClient->ip))))
	{
		if(!(aconf->status & CONF_EXEMPTDLINE))
		{
			exit_client(client_p, source_p, &me, "D-lined");
			return;
		}
	}

	sendto_one(source_p, "NOTICE * :CGI:IRC host/IP set to %s %s", parv[3], parv[4]);
}

static void
new_local_user(void *data)
{
	struct Client *source_p = data;
	struct ConfItem *aconf = source_p->localClient->att_conf;

	if (aconf == NULL)
		return;

	if (!irccmp(aconf->info.name, "webirc."))
		exit_client(source_p, source_p, &me, "Cannot log in using a WEBIRC block");
}
