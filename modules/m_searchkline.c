/*
 *  Copyright (C) 2020 Ed Kellett
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */
#include <stdinc.h>
#include <send.h>
#include <client.h>
#include <modules.h>
#include <msg.h>
#include "hash.h"
#include <hostmask.h>
#include <numeric.h>
#include <s_conf.h>
#include <s_newconf.h>
#include <reject.h>

static const char searchkline_desc[] = "Provides the ability to search for K/D-lines";

static void mo_searchkline(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message searchkline_msgtab = {
	"SEARCHKLINE", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_ignore, mg_ignore, mg_ignore, {mo_searchkline, 2}}
};

mapi_clist_av1 searchkline_clist[] = { &searchkline_msgtab, NULL };

DECLARE_MODULE_AV2(searchkline, NULL, NULL, searchkline_clist, NULL, NULL, NULL, NULL, searchkline_desc);

static void
report_kdline(struct Client *client, int type, struct ConfItem *aconf)
{
	char *puser, *phost, *reason, *operreason, *mask;
	char userhost[BUFSIZE];
	char reasonbuf[BUFSIZE];
	char letter;

	switch (type)
	{
	case CONF_KILL:
		letter = 'K';
		break;
	case CONF_DLINE:
		letter = 'D';
		break;
	}

	get_printable_kline(client, aconf, &phost, &reason, &puser, &operreason);
	if (operreason != NULL)
	{
		snprintf(reasonbuf, sizeof reasonbuf, "%s!%s", reason, operreason);
		reason = reasonbuf;
	}

	if (!EmptyString(aconf->user))
	{
		snprintf(userhost, sizeof userhost, "%s@%s", puser, phost);
		mask = userhost;
	}
	else
	{
		mask = phost;
	}

	sendto_one(client, form_str(RPL_TESTLINE),
		me.name, client->name,
		(aconf->flags & CONF_FLAGS_TEMPORARY) ? tolower(letter) : letter,
		(aconf->flags & CONF_FLAGS_TEMPORARY) ?
			(long) ((aconf->hold - rb_current_time()) / 60) : 0L,
		mask, reason);
}


static bool
search_ip_kdlines(struct Client *client, const char *username, struct sockaddr *ip, int blen, int fam)
{
	struct sockaddr_in ip4;
	bool found = false;
	struct AddressRec *arec;
	int masktype = fam == AF_INET ? HM_IPV4 : HM_IPV6;
	bool match_dlines = mask_match(username, "*");

	size_t i;
	size_t min = 0;
	size_t max = ARRAY_SIZE(atable);

	if (fam == AF_INET && blen == 32)
	{
		min = max = hash_ipv4(ip, 32);
	}
	else if (fam == AF_INET6 && blen == 128)
	{
		min = max = hash_ipv6(ip, 128);
	}

	for (i = min; i < max; i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if ((arec->type != CONF_DLINE && arec->type != CONF_KILL) ||
					arec->masktype != masktype ||
					arec->Mask.ipa.bits < blen ||
					!comp_with_mask_sock(ip, (struct sockaddr *)&arec->Mask.ipa.addr, blen))
				continue;

			if (arec->type == CONF_KILL && !mask_match(username, arec->username))
				continue;
			if (arec->type == CONF_DLINE && !match_dlines)
				continue;

			report_kdline(client, arec->type, arec->aconf);
			found = true;
		}
	}

	if (fam == AF_INET6 && blen == 128 &&
			rb_ipv4_from_ipv6((struct sockaddr_in6 *)ip, &ip4))
	{
		found = found || search_ip_kdlines(client, username, (struct sockaddr *)&ip4, 32, AF_INET);
	}

	return found;
}

static void
mo_searchkline(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct rb_sockaddr_storage ip;
	const char *username = NULL;
	const char *host = NULL;
	char *mask;
	char *p;
	int blen;
	int type;
	bool found = false;
	size_t i;
	struct AddressRec *arec;

	if (!HasPrivilege(source_p, "oper:testline"))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
			   me.name, source_p->name, "testline");
		return;
	}

	mask = LOCAL_COPY(parv[1]);

	if ((p = strchr(mask, '@')))
	{
		*p++ = '\0';
		username = mask;
		host = p;

		if(EmptyString(host))
			return;
	}
	else
	{
		host = mask;
	}

	if (username == NULL)
	{
		username = "*";
	}

	/* parses as an IP, check for IP bans */
	if ((type = parse_netmask(host, &ip, &blen)) != HM_HOST)
	{
		if(type == HM_IPV6)
			found = found || search_ip_kdlines(source_p, username, (struct sockaddr *)&ip, blen, AF_INET6);
		else
			found = found || search_ip_kdlines(source_p, username, (struct sockaddr *)&ip, blen, AF_INET);
	}

	/* all that's left is to check host-like K-lines */
	for (i = 0; i < ARRAY_SIZE(atable); i++)
	{
		for (arec = atable[i]; arec; arec = arec->next)
		{
			if (arec->type != CONF_KILL ||
					(type != HM_HOST && arec->masktype != HM_HOST) ||
					!mask_match(host, arec->aconf->host) ||
					!mask_match(username, arec->username))
				continue;

			report_kdline(source_p, arec->type, arec->aconf);
			found = true;
		}
	}

	if (found)
	{
		sendto_one(source_p, form_str(RPL_TESTLINE),
			me.name, source_p->name,
			'*', 0l, "*", "End of search results");
		return;
	}

	sendto_one(source_p, form_str(RPL_NOTESTLINE),
			me.name, source_p->name, parv[1]);
}
