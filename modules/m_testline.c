/* modules/m_testline.c
 *
 *  Copyright (C) 2004 Lee Hardy <lee@leeh.co.uk>
 *  Copyright (C) 2004-2005 ircd-ratbox development team
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
#include "send.h"
#include "client.h"
#include "modules.h"
#include "msg.h"
#include "hash.h"
#include "hostmask.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "reject.h"

static const char testline_desc[] = "Provides the ability to test I/K/D/X lines and RESVs";

static void mo_testline(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void mo_testkline(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void mo_testgecos(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message testline_msgtab = {
	"TESTLINE", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_testline, 2}}
};
struct Message testkline_msgtab = {
	"TESTKLINE", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_testkline, 2}}
};
struct Message testgecos_msgtab = {
	"TESTGECOS", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_testgecos, 2}}
};

mapi_clist_av1 testline_clist[] = { &testline_msgtab, &testkline_msgtab, &testgecos_msgtab, NULL };

DECLARE_MODULE_AV2(testline, NULL, NULL, testline_clist, NULL, NULL, NULL, NULL, testline_desc);

static void
mo_testline(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct ConfItem *aconf;
	struct ConfItem *resv_p;
	struct rb_sockaddr_storage ip;
	char user_trunc[USERLEN + 1], notildeuser_trunc[USERLEN + 1];
	const char *name = NULL;
	const char *username = NULL;
	const char *host = NULL;
	char *mask;
	char *p;
	int host_mask;
	int type;
	int duration;
	char *puser, *phost, *reason, *operreason;
	char reasonbuf[BUFSIZE];

	if (!HasPrivilege(source_p, "oper:testline"))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "testline");
		return;
	}

	mask = LOCAL_COPY(parv[1]);

	if (IsChannelName(mask))
	{
		resv_p = hash_find_resv(mask);
		if (resv_p != NULL)
		{
			sendto_one(source_p, form_str(RPL_TESTLINE),
					me.name, source_p->name,
					resv_p->hold ? 'q' : 'Q',
					resv_p->hold ? (long) ((resv_p->hold - rb_current_time()) / 60) : 0L,
					resv_p->host, resv_p->passwd);
			/* this is a false positive, so make sure it isn't counted in stats q
			 * --nenolod
			 */
			resv_p->port--;
		}
		else
			sendto_one(source_p, form_str(RPL_NOTESTLINE),
					me.name, source_p->name, parv[1]);
		return;
	}

	if((p = strchr(mask, '!')))
	{
		*p++ = '\0';
		name = mask;
		mask = p;

		if(EmptyString(mask))
		{
			sendto_one_notice(source_p, "Invalid syntax for TESTLINE");
			return;
		}
	}

	if((p = strchr(mask, '@')))
	{
		*p++ = '\0';
		username = mask;
		host = p;

		if(EmptyString(host))
		{
			sendto_one_notice(source_p, "Invalid syntax for TESTLINE");
			return;
		}
	}
	else
		host = mask;

	/* parses as an IP, check for a dline */
	if((type = parse_netmask(host, &ip, &host_mask)) != HM_HOST)
	{
		if(type == HM_IPV6)
			aconf = find_dline((struct sockaddr *)&ip, AF_INET6);
		else
			aconf = find_dline((struct sockaddr *)&ip, AF_INET);

		if(aconf && aconf->status & CONF_DLINE)
		{
			get_printable_kline(source_p, aconf, &phost, &reason, &puser, &operreason);
			snprintf(reasonbuf, sizeof(reasonbuf), "%s%s%s", reason,
				operreason ? "|" : "", operreason ? operreason : "");
			sendto_one(source_p, form_str(RPL_TESTLINE),
				me.name, source_p->name,
				(aconf->flags & CONF_FLAGS_TEMPORARY) ? 'd' : 'D',
				(aconf->flags & CONF_FLAGS_TEMPORARY) ?
				 (long) ((aconf->hold - rb_current_time()) / 60) : 0L,
				phost, reasonbuf);

			return;
		}
		/* Otherwise, aconf is an exempt{} */
		if(aconf == NULL &&
				(duration = is_reject_ip((struct sockaddr *)&ip)))
			sendto_one(source_p, form_str(RPL_TESTLINE),
					me.name, source_p->name,
					'!',
					duration / 60L,
					host, "Reject cache");
		if(aconf == NULL &&
				(duration = is_throttle_ip((struct sockaddr *)&ip)))
			sendto_one(source_p, form_str(RPL_TESTLINE),
					me.name, source_p->name,
					'!',
					duration / 60L,
					host, "Throttled");
	}

	if (username != NULL)
	{
		rb_strlcpy(user_trunc, username, sizeof user_trunc);
		rb_strlcpy(notildeuser_trunc, *username == '~' ? username + 1 : username, sizeof notildeuser_trunc);
	}
	else
	{
		rb_strlcpy(user_trunc, "dummy", sizeof user_trunc);
		rb_strlcpy(notildeuser_trunc, "dummy", sizeof notildeuser_trunc);
	}
	/* now look for a matching I/K/G */
	if((aconf = find_address_conf(host, NULL, user_trunc, notildeuser_trunc,
				(type != HM_HOST) ? (struct sockaddr *)&ip : NULL,
				(type != HM_HOST) ? (
				 (type == HM_IPV6) ? AF_INET6 :
				  AF_INET) : 0, NULL)))
	{
		static char buf[HOSTLEN+USERLEN+2];

		if(aconf->status & CONF_KILL)
		{
			get_printable_kline(source_p, aconf, &phost, &reason, &puser, &operreason);
			snprintf(buf, sizeof(buf), "%s@%s",
					puser, phost);
			snprintf(reasonbuf, sizeof(reasonbuf), "%s%s%s", reason,
				operreason ? "|" : "", operreason ? operreason : "");
			sendto_one(source_p, form_str(RPL_TESTLINE),
				me.name, source_p->name,
				(aconf->flags & CONF_FLAGS_TEMPORARY) ? 'k' : 'K',
				(aconf->flags & CONF_FLAGS_TEMPORARY) ?
				 (long) ((aconf->hold - rb_current_time()) / 60) : 0L,
				buf, reasonbuf);
			return;
		}
	}

	/* they asked us to check a nick, so hunt for resvs.. */
	if(name && (resv_p = find_nick_resv(name)))
	{
		sendto_one(source_p, form_str(RPL_TESTLINE),
				me.name, source_p->name,
				resv_p->hold ? 'q' : 'Q',
				resv_p->hold ? (long) ((resv_p->hold - rb_current_time()) / 60) : 0L,
				resv_p->host, resv_p->passwd);

		/* this is a false positive, so make sure it isn't counted in stats q
		 * --nenolod
		 */
		resv_p->port--;
		return;
	}

	/* no matching resv, we can print the I: if it exists */
	if(aconf && aconf->status & CONF_CLIENT)
	{
		sendto_one_numeric(source_p, RPL_STATSILINE, form_str(RPL_STATSILINE),
				aconf->info.name, EmptyString(aconf->spasswd) ? "<NULL>" : aconf->spasswd,
				show_iline_prefix(source_p, aconf, aconf->user),
				aconf->host, aconf->port, aconf->className,
				CheckEmpty(aconf->desc));
		return;
	}

	/* nothing matches.. */
	sendto_one(source_p, form_str(RPL_NOTESTLINE),
			me.name, source_p->name, parv[1]);
}


static void
mo_testkline(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct ConfItem *aconf;
	struct rb_sockaddr_storage ip;
	char user_trunc[USERLEN + 1];
	const char *username = NULL;
	const char *host = NULL;
	char *mask;
	char *p;
	int host_mask;
	int type;
	int duration;
	char *puser, *phost, *reason, *operreason;
	char reasonbuf[BUFSIZE];

	if (!HasPrivilege(source_p, "oper:testline"))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "testline");
		return;
	}

	mask = LOCAL_COPY(parv[1]);

	if ((p = strchr(mask, '!')))
	{
		mask = p + 1;

		if(EmptyString(mask))
		{
			sendto_one_notice(source_p, "Invalid syntax for TESTKLINE");
			return;
		}
	}

	if ((p = strchr(mask, '@')))
	{
		*p++ = '\0';
		username = mask;
		host = p;

		if(EmptyString(host))
		{
			sendto_one_notice(source_p, "Invalid syntax for TESTKLINE");
			return;
		}
	}
	else
	{
		host = mask;
	}

	/* parses as an IP, check for a dline */
	if ((type = parse_netmask(host, &ip, &host_mask)) != HM_HOST)
	{
		if(type == HM_IPV6)
			aconf = find_dline((struct sockaddr *)&ip, AF_INET6);
		else
			aconf = find_dline((struct sockaddr *)&ip, AF_INET);

		if(aconf && aconf->status & CONF_DLINE)
		{
			get_printable_kline(source_p, aconf, &phost, &reason, &puser, &operreason);
			snprintf(reasonbuf, sizeof(reasonbuf), "%s%s%s", reason,
				operreason ? "|" : "", operreason ? operreason : "");
			sendto_one(source_p, form_str(RPL_TESTLINE),
				me.name, source_p->name,
				(aconf->flags & CONF_FLAGS_TEMPORARY) ? 'd' : 'D',
				(aconf->flags & CONF_FLAGS_TEMPORARY) ?
				 (long) ((aconf->hold - rb_current_time()) / 60) : 0L,
				phost, reasonbuf);

			return;
		}
		/* Otherwise, aconf is an exempt{} */
		if (aconf == NULL && (duration = is_reject_ip((struct sockaddr *)&ip)))
			sendto_one(source_p, form_str(RPL_TESTLINE),
					me.name, source_p->name,
					'!',
					duration / 60L,
					host, "Reject cache");
		if (aconf == NULL && (duration = is_throttle_ip((struct sockaddr *)&ip)))
			sendto_one(source_p, form_str(RPL_TESTLINE),
					me.name, source_p->name,
					'!',
					duration / 60L,
					host, "Throttled");
	}

	if (username != NULL)
		rb_strlcpy(user_trunc, username, sizeof user_trunc);
	else
		rb_strlcpy(user_trunc, "dummy", sizeof user_trunc);

	aconf = find_conf_by_address(host,
		type != HM_HOST ? host : NULL,
		NULL,
		type != HM_HOST ? (struct sockaddr *)&ip : NULL,
		CONF_KILL,
		type == HM_IPV6 ? AF_INET6 : AF_INET,
		user_trunc, NULL);

	if (aconf != NULL && aconf->status & CONF_KILL)
	{
		static char buf[HOSTLEN+USERLEN+2];

		get_printable_kline(source_p, aconf, &phost, &reason, &puser, &operreason);
		snprintf(buf, sizeof(buf), "%s@%s",
				puser, phost);
		snprintf(reasonbuf, sizeof(reasonbuf), "%s%s%s", reason,
			operreason ? "|" : "", operreason ? operreason : "");
		sendto_one(source_p, form_str(RPL_TESTLINE),
			me.name, source_p->name,
			(aconf->flags & CONF_FLAGS_TEMPORARY) ? 'k' : 'K',
			(aconf->flags & CONF_FLAGS_TEMPORARY) ?
			 (long) ((aconf->hold - rb_current_time()) / 60) : 0L,
			buf, reasonbuf);
		return;
	}

	sendto_one(source_p, form_str(RPL_NOTESTLINE),
			me.name, source_p->name, parv[1]);
}

static void
mo_testgecos(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct ConfItem *aconf;

	if (!HasPrivilege(source_p, "oper:testline"))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "testline");
		return;
	}

	if(!(aconf = find_xline(parv[1], 0)))
	{
		sendto_one(source_p, form_str(RPL_NOTESTLINE),
				me.name, source_p->name, parv[1]);
		return;
	}

	sendto_one(source_p, form_str(RPL_TESTLINE),
			me.name, source_p->name,
			aconf->hold ? 'x' : 'X',
			aconf->hold ? (long) ((aconf->hold - rb_current_time()) / 60) : 0L,
			aconf->host, aconf->passwd);
}
