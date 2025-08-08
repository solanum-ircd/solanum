/*
 *  Solanum: a slightly advanced ircd
 *  hostmask.c: Code to efficiently find IP & hostmask based configs.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *  Copyright (C) 2005-2008 charybdis development team
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
#include "ircd_defs.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "hostmask.h"
#include "numeric.h"
#include "send.h"
#include "match.h"

static unsigned long hash_ipv6(struct sockaddr *, int);
static unsigned long hash_ipv4(struct sockaddr *, int);


static int
_parse_netmask(const char *text, struct rb_sockaddr_storage *naddr, int *nb, bool strict)
{
	char *ip = LOCAL_COPY(text);
	char *ptr;
	char *endp;
	struct rb_sockaddr_storage *addr, xaddr;
	int *b, xb;
	if(nb == NULL)
		b = &xb;
	else
		b = nb;

	if(naddr == NULL)
		addr = &xaddr;
	else
		addr = naddr;

	if(strpbrk(ip, "*?") != NULL)
	{
		return HM_HOST;
	}
	if(strchr(ip, ':'))
	{
		if((ptr = strchr(ip, '/')))
		{
			*ptr = '\0';
			ptr++;
			long n = strtol(ptr, &endp, 10);
			if (endp == ptr || n < 0)
				return HM_HOST;
			if (n > 128 || *endp != '\0')
			{
				if (strict)
					return HM_ERROR;
				else
					n = 128;
			}
			*b = n;
		} else
			*b = 128;
		if(rb_inet_pton_sock(ip, addr) > 0)
			return HM_IPV6;
		else
			return HM_HOST;
	} else
	if(strchr(text, '.'))
	{
		if((ptr = strchr(ip, '/')))
		{
			*ptr = '\0';
			ptr++;
			long n = strtol(ptr, &endp, 10);
			if (endp == ptr || n < 0)
				return HM_HOST;
			if (n > 32 || *endp != '\0')
			{
				if (strict)
					return HM_ERROR;
				else
					n = 32;
			}
			*b = n;
		} else
			*b = 32;
		if(rb_inet_pton_sock(ip, addr) > 0)
			return HM_IPV4;
		else
			return HM_HOST;
	}
	return HM_HOST;
}

/* int parse_netmask(const char *, struct rb_sockaddr_storage *, int *);
 * Input: A hostmask, or an IPV4/6 address.
 * Output: An integer describing whether it is an IPV4, IPV6 address or a
 *         hostmask, an address(if it is an IP mask),
 *         a bitlength(if it is IP mask).
 * Side effects: None
 */
int parse_netmask(const char *mask, struct rb_sockaddr_storage *addr, int *blen)
{
	return _parse_netmask(mask, addr, blen, false);
}

int parse_netmask_strict(const char *mask, struct rb_sockaddr_storage *addr, int *blen)
{
	return _parse_netmask(mask, addr, blen, true);
}

/* Hashtable stuff...now external as its used in m_stats.c */
struct AddressRec *atable[ATABLE_SIZE];

void
init_host_hash(void)
{
	memset(&atable, 0, sizeof(atable));
}

/* unsigned long hash_ipv4(struct rb_sockaddr_storage*)
 * Input: An IP address.
 * Output: A hash value of the IP address.
 * Side effects: None
 */
static unsigned long
hash_ipv4(struct sockaddr *saddr, int bits)
{
	struct sockaddr_in *addr = (struct sockaddr_in *)(void *)saddr;

	if(bits != 0)
	{
		unsigned long av = ntohl(addr->sin_addr.s_addr) & ~((1 << (32 - bits)) - 1);
		return (av ^ (av >> 12) ^ (av >> 24)) & (ATABLE_SIZE - 1);
	}

	return 0;
}

/* unsigned long hash_ipv6(struct rb_sockaddr_storage*)
 * Input: An IP address.
 * Output: A hash value of the IP address.
 * Side effects: None
 */
static unsigned long
hash_ipv6(struct sockaddr *saddr, int bits)
{
	struct sockaddr_in6 *addr = (struct sockaddr_in6 *)(void *)saddr;
	unsigned long v = 0, n;
	for (n = 0; n < 16; n++)
	{
		if(bits >= 8)
		{
			v ^= addr->sin6_addr.s6_addr[n];
			bits -= 8;
		}
		else if(bits)
		{
			v ^= addr->sin6_addr.s6_addr[n] & ~((1 << (8 - bits)) - 1);
			return v & (ATABLE_SIZE - 1);
		}
		else
			return v & (ATABLE_SIZE - 1);
	}
	return v & (ATABLE_SIZE - 1);
}

/* int hash_text(const char *start)
 * Input: The start of the text to hash.
 * Output: The hash of the string between 1 and (TH_MAX-1)
 * Side-effects: None.
 */
static int
hash_text(const char *start)
{
	const char *p = start;
	unsigned long h = 0;

	while(*p)
	{
		h = (h << 4) - (h + (unsigned char) irctolower(*p++));
	}

	return (h & (ATABLE_SIZE - 1));
}

/* unsigned long get_hash_mask(const char *)
 * Input: The text to hash.
 * Output: The hash of the string right of the first '.' past the last
 *         wildcard in the string.
 * Side-effects: None.
 */
static unsigned long
get_mask_hash(const char *text)
{
	const char *hp = "", *p;

	for (p = text + strlen(text) - 1; p >= text; p--)
		if(*p == '*' || *p == '?')
			return hash_text(hp);
		else if(*p == '.')
			hp = p + 1;
	return hash_text(text);
}

/* struct ConfItem* find_conf_by_address(const char*, struct rb_sockaddr_storage*,
 *         int type, int fam, const char *username)
 *
 * This process needs to be kept in sync with check_one_kline().
 *
 * Input: The hostname, the address, the type of mask to find, the address
 *        family, the username.
 * Output: The matching value with the highest precedence.
 * Side-effects: None
 * Note: Setting bit 0 of the type means that the username is ignored.
 */
struct ConfItem *
find_conf_by_address(const char *name, const char *sockhost,
			const char *orighost,
			struct sockaddr *addr, int type, int fam,
			const char *username, const char *auth_user)
{
	unsigned long hprecv = 0;
	struct ConfItem *hprec = NULL;
	struct AddressRec *arec;
	struct sockaddr_in ip4;
	struct sockaddr *pip4 = NULL;
	int b;

	if(username == NULL)
		username = "";

	if(addr)
	{
		if (fam == AF_INET)
			pip4 = addr;

		if (fam == AF_INET6)
		{
			if (type == CONF_KILL && rb_ipv4_from_ipv6((struct sockaddr_in6 *)addr, &ip4))
				pip4 = (struct sockaddr *)&ip4;

			for (b = 128; b >= 0; b -= 16)
			{
				for (arec = atable[hash_ipv6(addr, b)]; arec; arec = arec->next)
					if(arec->type == (type & ~0x1) &&
					   arec->masktype == HM_IPV6 &&
					   comp_with_mask_sock(addr, (struct sockaddr *)&arec->Mask.ipa.addr,
						arec->Mask.ipa.bits) &&
						(type & 0x1 || match(arec-> username, username)) &&
						(type != CONF_CLIENT || !arec->auth_user ||
						(auth_user && match(arec->auth_user, auth_user))) &&
						arec->precedence > hprecv)
					{
						hprecv = arec->precedence;
						hprec = arec->aconf;
					}
			}
		}

		if (pip4 != NULL)
		{
			for (b = 32; b >= 0; b -= 8)
			{
				for (arec = atable[hash_ipv4(pip4, b)]; arec; arec = arec->next)
					if(arec->type == (type & ~0x1) &&
					   arec->masktype == HM_IPV4 &&
					   comp_with_mask_sock(pip4, (struct sockaddr *)&arec->Mask.ipa.addr,
							       arec->Mask.ipa.bits) &&
						(type & 0x1 || match(arec->username, username)) &&
						(type != CONF_CLIENT || !arec->auth_user ||
						(auth_user && match(arec->auth_user, auth_user))) &&
						arec->precedence > hprecv)
					{
						hprecv = arec->precedence;
						hprec = arec->aconf;
					}
			}
		}
	}

	if(orighost != NULL)
	{
		const char *p;

		for (p = orighost; p != NULL;)
		{
			for (arec = atable[hash_text(p)]; arec; arec = arec->next)

				if((arec->type == (type & ~0x1)) &&
				   (arec->masktype == HM_HOST) &&
				   arec->precedence > hprecv &&
				   match(arec->Mask.hostname, orighost) &&
				   (type != CONF_CLIENT || !arec->auth_user ||
				   (auth_user && match(arec->auth_user, auth_user))) &&
				   (type & 0x1 || match(arec->username, username)))
				{
					hprecv = arec->precedence;
					hprec = arec->aconf;
				}
			p = strchr(p, '.');
			if(p != NULL)
				p++;
			else
				break;
		}
		for (arec = atable[0]; arec; arec = arec->next)
		{
			if(arec->type == (type & ~0x1) &&
			   arec->masktype == HM_HOST &&
			   arec->precedence > hprecv &&
			   (match(arec->Mask.hostname, orighost) ||
			    (sockhost && match(arec->Mask.hostname, sockhost))) &&
			    (type != CONF_CLIENT || !arec->auth_user ||
			    (auth_user && match(arec->auth_user, auth_user))) &&
			   (type & 0x1 || match(arec->username, username)))
			{
				hprecv = arec->precedence;
				hprec = arec->aconf;
			}
		}
	}

	if(name != NULL)
	{
		const char *p;
		/* And yes - we have to check p after strchr and p after increment for
		 * NULL -kre */
		for (p = name; p != NULL;)
		{
			for (arec = atable[hash_text(p)]; arec; arec = arec->next)
				if((arec->type == (type & ~0x1)) &&
				   (arec->masktype == HM_HOST) &&
				   arec->precedence > hprecv &&
				   match(arec->Mask.hostname, name) &&
				   (type != CONF_CLIENT || !arec->auth_user ||
				   (auth_user && match(arec->auth_user, auth_user))) &&
				   (type & 0x1 || match(arec->username, username)))
				{
					hprecv = arec->precedence;
					hprec = arec->aconf;
				}
			p = strchr(p, '.');
			if(p != NULL)
				p++;
			else
				break;
		}
		for (arec = atable[0]; arec; arec = arec->next)
		{
			if(arec->type == (type & ~0x1) &&
			   arec->masktype == HM_HOST &&
			   arec->precedence > hprecv &&
			   (match(arec->Mask.hostname, name) ||
			    (sockhost && match(arec->Mask.hostname, sockhost))) &&
			    (type != CONF_CLIENT || !arec->auth_user ||
			    (auth_user && match(arec->auth_user, auth_user))) &&
			   (type & 0x1 || match(arec->username, username)))
			{
				hprecv = arec->precedence;
				hprec = arec->aconf;
			}
		}
	}
	return hprec;
}

/* struct ConfItem* find_address_conf(const char*, const char*,
 * 	                               struct rb_sockaddr_storage*, int);
 * Input: The hostname, username, address, address family.
 * Output: The applicable ConfItem.
 * Side-effects: None
 */
struct ConfItem *
find_address_conf(const char *host, const char *sockhost, const char *user,
		const char *notildeuser, struct sockaddr *ip, int aftype, char *auth_user)
{
	struct ConfItem *iconf, *kconf;
	const char *vuser;

	/* Find the best I-line... If none, return NULL -A1kmm */
	if(!(iconf = find_conf_by_address(host, sockhost, NULL, ip, CONF_CLIENT, aftype, user, auth_user)))
		return NULL;
	/* Find what their visible username will be.
	 * Note that the username without tilde may contain one char more.
	 * -- jilles */
	vuser = IsNoTilde(iconf) ? notildeuser : user;

	/* If they are exempt from K-lines, return the best I-line. -A1kmm */
	if(IsConfExemptKline(iconf))
		return iconf;

	/* if theres a spoof, check it against klines.. */
	if(IsConfDoSpoofIp(iconf))
	{
		char *p = strchr(iconf->info.name, '@');

		/* note, we dont need to pass sockhost here, as its
		 * guaranteed to not match by whats below.. --anfl
		 */
		if(p)
		{
			*p = '\0';
			kconf = find_conf_by_address(p+1, NULL, NULL, NULL, CONF_KILL, aftype, iconf->info.name, NULL);
			*p = '@';
		}
		else
			kconf = find_conf_by_address(iconf->info.name, NULL, NULL, NULL, CONF_KILL, aftype, vuser, NULL);

		if(kconf)
			return kconf;

		/* everything else checks real hosts, if they're kline_spoof_ip we're done */
		if(IsConfKlineSpoof(iconf))
			return iconf;
	}

	/* Find the best K-line... -A1kmm */
	kconf = find_conf_by_address(host, sockhost, NULL, ip, CONF_KILL, aftype, user, NULL);

	/* If they are K-lined, return the K-line */
	if(kconf)
		return kconf;

	/* if no_tilde, check the username without tilde against klines too
	 * -- jilles */
	if(user != vuser)
	{
		kconf = find_conf_by_address(host, sockhost, NULL, ip, CONF_KILL, aftype, vuser, NULL);
		if(kconf)
			return kconf;
	}

	return iconf;
}

/* struct ConfItem* find_dline(struct rb_sockaddr_storage*, int)
 * Input: An address, an address family.
 * Output: The best matching D-line or exempt line.
 * Side effects: None.
 */
struct ConfItem *
find_dline(struct sockaddr *addr, int aftype)
{
	struct ConfItem *aconf;
	struct sockaddr_in addr2;

	aconf = find_conf_by_address(NULL, NULL, NULL, addr, CONF_EXEMPTDLINE | 1, aftype, NULL, NULL);
	if(aconf)
		return aconf;
	aconf = find_conf_by_address(NULL, NULL, NULL, addr, CONF_DLINE | 1, aftype, NULL, NULL);
	if(aconf)
		return aconf;
	if(addr->sa_family == AF_INET6 &&
			rb_ipv4_from_ipv6((const struct sockaddr_in6 *)(const void *)addr, &addr2))
	{
		aconf = find_conf_by_address(NULL, NULL, NULL, (struct sockaddr *)&addr2, CONF_DLINE | 1, AF_INET, NULL, NULL);
		if(aconf)
			return aconf;
	}
	return NULL;
}

struct ConfItem *
find_exact_conf_by_address_filtered(const char *address, int type, const char *username, bool (*filter)(struct ConfItem *))
{
	int masktype, bits;
	unsigned long hv;
	struct AddressRec *arec;
	struct rb_sockaddr_storage addr;

	if(address == NULL)
		address = "/NOMATCH!/";
	masktype = parse_netmask(address, &addr, &bits);
	if(masktype == HM_IPV6)
	{
		/* We have to do this, since we do not re-hash for every bit -A1kmm. */
		hv = hash_ipv6((struct sockaddr *)&addr, bits - bits % 16);
	}
	else if(masktype == HM_IPV4)
	{
		/* We have to do this, since we do not re-hash for every bit -A1kmm. */
		hv = hash_ipv4((struct sockaddr *)&addr, bits - bits % 8);
	}
	else
	{
		hv = get_mask_hash(address);
	}
	for (arec = atable[hv]; arec; arec = arec->next)
	{
		if (arec->type == type &&
				arec->masktype == masktype &&
				(arec->username == NULL || username == NULL ? arec->username == username : !irccmp(arec->username, username)))
		{
			if (filter && !filter(arec->aconf))
				continue;

			if (masktype == HM_HOST)
			{
				if (!irccmp(arec->Mask.hostname, address))
					return arec->aconf;
			}
			else
			{
				if (arec->Mask.ipa.bits == bits &&
					comp_with_mask_sock((struct sockaddr *)&arec->Mask.ipa.addr, (struct sockaddr *)&addr, bits))
					return arec->aconf;
			}
		}
	}
	return NULL;
}

/* void find_exact_conf_by_address(const char*, int, const char *)
 * Input:
 * Output: ConfItem if found
 * Side-effects: None
 */
struct ConfItem *
find_exact_conf_by_address(const char *address, int type, const char *username)
{
	return find_exact_conf_by_address_filtered(address, type, username, NULL);
}

/* void add_conf_by_address(const char*, int, const char *,
 *         struct ConfItem *aconf)
 * Input:
 * Output: None
 * Side-effects: Adds this entry to the hash table.
 */
void
add_conf_by_address(const char *address, int type, const char *username, const char *auth_user, struct ConfItem *aconf)
{
	static unsigned long prec_value = 0xFFFFFFFF;
	int bits;
	unsigned long hv;
	struct AddressRec *arec;

	if(address == NULL)
		address = "/NOMATCH!/";
	arec = rb_malloc(sizeof(struct AddressRec));
	arec->masktype = parse_netmask(address, &arec->Mask.ipa.addr, &bits);
	if(arec->masktype == HM_IPV6)
	{
		arec->Mask.ipa.bits = bits;
		/* We have to do this, since we do not re-hash for every bit -A1kmm. */
		bits -= bits % 16;
		arec->next = atable[(hv = hash_ipv6((struct sockaddr *)&arec->Mask.ipa.addr, bits))];
		atable[hv] = arec;
	}
	else if(arec->masktype == HM_IPV4)
	{
		arec->Mask.ipa.bits = bits;
		/* We have to do this, since we do not re-hash for every bit -A1kmm. */
		bits -= bits % 8;
		arec->next = atable[(hv = hash_ipv4((struct sockaddr *)&arec->Mask.ipa.addr, bits))];
		atable[hv] = arec;
	}
	else
	{
		arec->Mask.hostname = address;
		arec->next = atable[(hv = get_mask_hash(address))];
		atable[hv] = arec;
	}
	arec->username = username;
	arec->auth_user = auth_user;
	arec->aconf = aconf;
	arec->precedence = prec_value--;
	arec->type = type;
}

/* void delete_one_address(const char*, struct ConfItem*)
 * Input: An address string, the associated ConfItem.
 * Output: None
 * Side effects: Deletes an address record. Frees the ConfItem if there
 *               is nothing referencing it, sets it as illegal otherwise.
 */
void
delete_one_address_conf(const char *address, struct ConfItem *aconf)
{
	int masktype, bits;
	unsigned long hv;
	struct AddressRec *arec, *arecl = NULL;
	struct rb_sockaddr_storage addr;
	masktype = parse_netmask(address, &addr, &bits);
	if(masktype == HM_IPV6)
	{
		/* We have to do this, since we do not re-hash for every bit -A1kmm. */
		bits -= bits % 16;
		hv = hash_ipv6((struct sockaddr *)&addr, bits);
	}
	else if(masktype == HM_IPV4)
	{
		/* We have to do this, since we do not re-hash for every bit -A1kmm. */
		bits -= bits % 8;
		hv = hash_ipv4((struct sockaddr *)&addr, bits);
	}
	else
		hv = get_mask_hash(address);
	for (arec = atable[hv]; arec; arec = arec->next)
	{
		if(arec->aconf == aconf)
		{
			if(arecl)
				arecl->next = arec->next;
			else
				atable[hv] = arec->next;
			aconf->status |= CONF_ILLEGAL;
			if(!aconf->clients)
				free_conf(aconf);
			rb_free(arec);
			return;
		}
		arecl = arec;
	}
}

/* void clear_out_address_conf(void)
 * Input: None
 * Output: None
 * Side effects: Clears out all address records in the hash table,
 *               frees them, and frees the ConfItems if nothing references
 *               them, otherwise sets them as illegal.
 */
void
clear_out_address_conf(enum aconf_category clear_type)
{
	int i;
	struct AddressRec **store_next;
	struct AddressRec *arec, *arecn;

	for (i = 0; i < ATABLE_SIZE; i++)
	{
		store_next = &atable[i];
		for (arec = atable[i]; arec; arec = arecn)
		{
			enum aconf_category cur_type;
			arecn = arec->next;

			if (arec->type == CONF_CLIENT || arec->type == CONF_EXEMPTDLINE || arec->type == CONF_SECURE)
				cur_type = AC_CONFIG;
			else
				cur_type = AC_BANDB;

			/* We keep the temporary K-lines and destroy the
			 * permanent ones, just to be confusing :) -A1kmm */
			if (arec->aconf->flags & CONF_FLAGS_TEMPORARY || cur_type != clear_type)
			{
				*store_next = arec;
				store_next = &arec->next;
			}
			else
			{
				arec->aconf->status |= CONF_ILLEGAL;
				if(!arec->aconf->clients)
					free_conf(arec->aconf);
				rb_free(arec);
			}
		}
		*store_next = NULL;
	}
}

/*
 * show_iline_prefix()
 *
 * inputs       - pointer to struct Client requesting output
 *              - pointer to struct ConfItem
 *              - name to which iline prefix will be prefixed to
 * output       - pointer to static string with prefixes listed in ascii form
 * side effects - NONE
 */
char *
show_iline_prefix(struct Client *sptr, struct ConfItem *aconf, char *name)
{
	static char prefix_of_host[USERLEN + 15];
	char *prefix_ptr;

	prefix_ptr = prefix_of_host;
	if(IsNoTilde(aconf))
		*prefix_ptr++ = '-';
	if(IsNeedIdentd(aconf))
		*prefix_ptr++ = '+';
	if(IsConfDoSpoofIp(aconf))
		*prefix_ptr++ = '=';
	if(IsNeedSasl(aconf))
		*prefix_ptr++ = '%';
	if(IsOper(sptr) && IsConfExemptFlood(aconf))
		*prefix_ptr++ = '|';
	if(IsOper(sptr) && IsConfExemptDNSBL(aconf) && !IsConfExemptKline(aconf))
		*prefix_ptr++ = '$';
	if(IsOper(sptr) && IsConfExemptKline(aconf))
		*prefix_ptr++ = '^';
	if(IsOper(sptr) && IsConfExemptLimits(aconf))
		*prefix_ptr++ = '>';
	rb_strlcpy(prefix_ptr, name, USERLEN + 1);
	return (prefix_of_host);
}

/* report_auth()
 *
 * Inputs: pointer to client to report to
 * Output: None
 * Side effects: Reports configured auth{} blocks to client_p
 */
void
report_auth(struct Client *client_p)
{
	char *name, *host, *user, *classname, *desc;
	const char *pass;
	struct AddressRec *arec;
	struct ConfItem *aconf;
	int i, port;

	for (i = 0; i < ATABLE_SIZE; i++)
		for (arec = atable[i]; arec; arec = arec->next)
			if(arec->type == CONF_CLIENT)
			{
				aconf = arec->aconf;

				if(!IsOperGeneral(client_p) && IsConfDoSpoofIp(aconf))
					continue;

				get_printable_conf(aconf, &name, &host, &pass, &user, &port,
						   &classname, &desc);

				if(!EmptyString(aconf->spasswd))
					pass = aconf->spasswd;

				sendto_one_numeric(client_p, RPL_STATSILINE,
						   form_str(RPL_STATSILINE),
						   name, pass, show_iline_prefix(client_p, aconf, user),
						   show_ip_conf(aconf, client_p) ? host : "255.255.255.255",
						   port, classname, desc);
			}
}

