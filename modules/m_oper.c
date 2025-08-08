/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_oper.c: Makes a user an IRC Operator.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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

#include "stdinc.h"
#include "client.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "logger.h"
#include "s_user.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "packet.h"
#include "cache.h"

static const char oper_desc[] = "Provides the OPER command to become an IRC operator";

static void m_oper(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void mc_oper(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void me_oper(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

static bool match_oper_password(const char *password, struct oper_conf *oper_p);

struct Message oper_msgtab = {
	"OPER", 0, 0, 0, 0,
	{mg_unreg, {m_oper, 3}, {mc_oper, 2}, mg_ignore, {me_oper, 2}, {m_oper, 3}}
};

mapi_clist_av1 oper_clist[] = { &oper_msgtab, NULL };

DECLARE_MODULE_AV2(oper, NULL, NULL, oper_clist, NULL, NULL, NULL, NULL, oper_desc);

/*
 * m_oper
 *      parv[1] = oper name
 *      parv[2] = oper password
 */
static void
m_oper(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct oper_conf *oper_p;
	const char *name;
	const char *password;

	name = parv[1];
	password = parv[2];

	if (ConfigFileEntry.oper_secure_only && !IsSecureClient(source_p))
	{
		sendto_one_notice(source_p, ":You must be using a secure connection to /OPER on this server");
		if (ConfigFileEntry.failed_oper_notice)
		{
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
					"Failed OPER attempt - missing secure connection by %s (%s@%s)",
					source_p->name, source_p->username, source_p->host);
		}
		return;
	}

	if(IsOper(source_p))
	{
		sendto_one(source_p, form_str(RPL_YOUREOPER), me.name, source_p->name);
		send_oper_motd(source_p);
		return;
	}

	/* end the grace period */
	if(!IsFloodDone(source_p))
		flood_endgrace(source_p);

	oper_p = find_oper_conf(source_p->username, source_p->orighost,
				source_p->sockhost, name);

	if(oper_p == NULL)
	{
		sendto_one_numeric(source_p, ERR_NOOPERHOST, form_str(ERR_NOOPERHOST));
		ilog(L_FOPER, "FAILED OPER (%s) by (%s!%s@%s) (%s)",
		     name, source_p->name,
		     source_p->username, source_p->host, source_p->sockhost);

		if(ConfigFileEntry.failed_oper_notice)
		{
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
					     "Failed OPER attempt - user@host mismatch or no operator block for %s by %s (%s@%s)",
					     name, source_p->name, source_p->username, source_p->host);
		}

		return;
	}

	if(IsOperConfNeedSSL(oper_p) && !IsSecureClient(source_p))
	{
		sendto_one_numeric(source_p, ERR_NOOPERHOST, form_str(ERR_NOOPERHOST));
		ilog(L_FOPER, "FAILED OPER (%s) by (%s!%s@%s) (%s) -- requires SSL/TLS",
		     name, source_p->name,
		     source_p->username, source_p->host, source_p->sockhost);

		if(ConfigFileEntry.failed_oper_notice)
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					     "Failed OPER attempt - missing SSL/TLS by %s (%s@%s)",
					     source_p->name, source_p->username, source_p->host);
		}
		return;
	}

	if (oper_p->certfp != NULL)
	{
		if (source_p->certfp == NULL || rb_strcasecmp(source_p->certfp, oper_p->certfp))
		{
			sendto_one_numeric(source_p, ERR_NOOPERHOST, form_str(ERR_NOOPERHOST));
			ilog(L_FOPER, "FAILED OPER (%s) by (%s!%s@%s) (%s) -- client certificate fingerprint mismatch",
			     name, source_p->name,
			     source_p->username, source_p->host, source_p->sockhost);

			if(ConfigFileEntry.failed_oper_notice)
			{
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						     "Failed OPER attempt - client certificate fingerprint mismatch by %s (%s@%s)",
						     source_p->name, source_p->username, source_p->host);
			}
			return;
		}
	}

	if(match_oper_password(password, oper_p))
	{
		oper_up(source_p, oper_p);

		ilog(L_OPERED, "OPER %s by %s!%s@%s (%s)",
		     name, source_p->name, source_p->username, source_p->host,
		     source_p->sockhost);
		return;
	}
	else
	{
		sendto_one(source_p, form_str(ERR_PASSWDMISMATCH),
			   me.name, source_p->name);

		ilog(L_FOPER, "FAILED OPER (%s) by (%s!%s@%s) (%s)",
		     name, source_p->name, source_p->username, source_p->host,
		     source_p->sockhost);

		if(ConfigFileEntry.failed_oper_notice)
		{
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
					     "Failed OPER attempt by %s (%s@%s)",
					     source_p->name, source_p->username, source_p->host);
		}
	}
}

/*
 * mc_oper - server-to-server OPER propagation
 *     parv[1] = opername
 *     parv[2] = privset
 */
static void
mc_oper(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct PrivilegeSet *privset;

	if (parc >= 3)
	{
		sendto_server(client_p, NULL, CAP_TS6, NOCAPS, ":%s OPER %s %s", use_id(source_p), parv[1], parv[2]);

		privset = privilegeset_get(parv[2]);
		if(privset == NULL)
		{
			/* if we don't have a matching privset, we'll create an empty one and
			 * mark it illegal, so it gets picked up on a rehash later */
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "Received OPER for %s with unknown privset %s", source_p->name, parv[2]);
			privset = privilegeset_set_new(parv[2], "", 0);
			privset->status |= CONF_ILLEGAL;
		}

		privset = privilegeset_ref(privset);
		if (source_p->user->privset != NULL)
			privilegeset_unref(source_p->user->privset);

		source_p->user->privset = privset;
	}
	else
	{
		sendto_server(client_p, NULL, CAP_TS6, NOCAPS, ":%s OPER %s", use_id(source_p), parv[1]);
	}

	rb_free(source_p->user->opername);
	source_p->user->opername = rb_strdup(parv[1]);
}

/*
 * me_oper - ircd-seven-style OPER propagation
 *     parv[1] = opername
 */
static void
me_oper(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	rb_free(source_p->user->opername);
	source_p->user->opername = rb_strdup(parv[1]);
}

/*
 * match_oper_password
 *
 * inputs       - pointer to given password
 *              - pointer to Conf
 * output       - true if match, false otherwise
 * side effects - none
 */
static bool
match_oper_password(const char *password, struct oper_conf *oper_p)
{
	const char *encr;

	/* passwd may be NULL pointer. Head it off at the pass... */
	if(EmptyString(oper_p->passwd))
		return false;

	if(IsOperConfEncrypted(oper_p))
	{
		/* use first two chars of the password they send in as salt */
		/* If the password in the conf is MD5, and ircd is linked
		 * to scrypt on FreeBSD, or the standard crypt library on
		 * glibc Linux, then this code will work fine on generating
		 * the proper encrypted hash for comparison.
		 */
		if(!EmptyString(password))
			encr = rb_crypt(password, oper_p->passwd);
		else
			encr = "";
	}
	else
		encr = password;

	if(encr != NULL && strcmp(encr, oper_p->passwd) == 0)
		return true;
	else
		return false;
}
