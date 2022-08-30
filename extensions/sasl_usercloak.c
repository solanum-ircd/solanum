#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "hostmask.h"
#include "ircd.h"
#include "send.h"
#include "hash.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"

#include <stdint.h>

static const char sasl_usercloak_desc[] =
	"Insert the SASL account name into certain iline spoofed hosts";

static void check_new_user(void *data);
mapi_hfn_list_av1 sasl_usercloak_hfnlist[] = {
	{ "new_local_user", check_new_user },
	{ NULL, NULL }
};


unsigned int fnv_hash_string(char *str)
{
	unsigned int hash = 0x811c9dc5; // Magic value for 32-bit fnv1 hash initialisation.
	unsigned char *p = (unsigned char *)str;
	while (*p)
	{
		hash += (hash<<1) + (hash<<4) + (hash<<7) + (hash<<8) + (hash<<24);
		hash ^= *p++;
	}
	return hash;
}

static void
check_new_user(void *vdata)
{
	struct Client *source_p = vdata;

	if (IsAnyDead(source_p))
		return;

	if (!IsIPSpoof(source_p))
		return;

	if (EmptyString(source_p->user->suser))
		return;

	char *accountpart = strstr(source_p->orighost, "/account");
	if (!accountpart || accountpart[8] != '\0')
		return;

	accountpart += 1;

	char buf[HOSTLEN];
	memset(buf, 0, sizeof(buf));
	char *dst = buf;

	strncpy(buf, source_p->orighost, accountpart - source_p->orighost);
	dst += accountpart - source_p->orighost;

	int needhash = 0;

	for (char *src = source_p->user->suser; *src ; src++ )
	{
		if (dst >= buf + sizeof(buf))
		{
			/* Doesn't fit. Warn opers and bail. */
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
					"Couldn't fit account name part %s in hostname for %s!%s@%s",
					source_p->user->suser, source_p->name, source_p->username, source_p->orighost);
			return;
		}

		char c = tolower(*src);

		if (IsHostChar(c))
			*dst++ = c;
		else
			needhash = 1;
	}

	if (needhash)
	{
		if (dst > buf + sizeof(buf) - 12) /* '/x-' plus eight digit hash plus null terminator */
		{
			/* Doesn't fit. Warn opers and bail. */
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
					"Couldn't fit account name part %s in hostname for %s!%s@%s",
					source_p->user->suser, source_p->name, source_p->username, source_p->orighost);
			return;
		}

		*dst++ = '/';
		*dst++ = 'x';
		*dst++ = '-';

		unsigned int hashval = fnv_hash_string(source_p->user->suser);
		hashval %= 100000000; // eight digits only please.
		snprintf(dst, 9, "%08u", hashval);
	}

	/* just in case */
	buf[HOSTLEN-1] = '\0';

	/* If hostname has been changed already (probably by services cloak on SASL login), then
	 * leave it intact. If not, change it. In either case, update the original hostname.
	 */
	if (0 == irccmp(source_p->host, source_p->orighost))
		change_nick_user_host(source_p, source_p->name, source_p->username, buf, 0, "Changing host");
	strncpy(source_p->orighost, buf, HOSTLEN);

	{
		struct ConfItem *aconf = find_kline(source_p);

		if(aconf == NULL)
			return;

		if(IsExemptKline(source_p))
		{
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
						 "KLINE over-ruled for %s, client is kline_exempt [%s@%s]",
						 get_client_name(source_p, HIDE_IP),
						 aconf->user, aconf->host);
			return;
		}

		sendto_realops_snomask(SNO_BANNED, L_NETWIDE,
			"Rejecting K-Lined user %s [%s] (%s@%s)", get_client_name(source_p, HIDE_IP),
			show_ip(NULL, source_p) ? source_p->sockhost : "255.255.255.255", aconf->user, aconf->host);

		notify_banned_client(source_p, aconf, K_LINED);
	}
}

DECLARE_MODULE_AV2(sasl_usercloak, NULL, NULL, NULL, NULL, sasl_usercloak_hfnlist, NULL, NULL, sasl_usercloak_desc);
