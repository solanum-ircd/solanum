/*
 * Solanum: a slightly advanced ircd
 * dnsbl.c: Manages DNSBL entries and lookups
 *
 * Copyright (C) 2006-2011 charybdis development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
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

/* Originally written for charybdis circa 2006 (by nenolod?).
 * Tweaked for authd. Some functions and structs renamed. Public/private
 * interfaces have been shifted around. Some code has been cleaned up too.
 * -- Elizafox 24 March 2016
 */

#include "authd.h"
#include "defaults.h"
#include "provider.h"
#include "notice.h"
#include "stdinc.h"
#include "dns.h"

#define SELF_PID (dnsbl_provider.id)

typedef enum filter_t
{
	FILTER_ALL = 1,
	FILTER_LAST = 2,
} filter_t;

/* dnsbl accepted IP types */
#define IPTYPE_IPV4	1
#define IPTYPE_IPV6	2

/* A configured DNSBL */
struct dnsbl
{
	char host[IRCD_RES_HOSTLEN + 1];
	char reason[BUFSIZE];		/* Reason template (ircd fills in the blanks) */
	uint8_t iptype;			/* IP types supported */
	rb_dlink_list filters;		/* Filters for queries */

	bool delete;			/* If true delete when no clients */
	int refcount;			/* When 0 and delete is set, remove this dnsbl */
	unsigned int hits;

	time_t lastwarning;		/* Last warning about garbage replies sent */
};

/* A lookup in progress for a particular DNSBL for a particular client */
struct dnsbl_lookup
{
	struct dnsbl *bl;		/* dnsbl we're checking */
	struct auth_client *auth;	/* Client */
	struct dns_query *query;	/* DNS query pointer */

	rb_dlink_node node;
};

/* A dnsbl filter */
struct dnsbl_filter
{
	filter_t type;			/* Type of filter */
	char filter[HOSTIPLEN];		/* The filter itself */

	rb_dlink_node node;
};

/* dnsbl user data attached to auth_client instance */
struct dnsbl_user
{
	bool started;
	rb_dlink_list queries;		/* dnsbl queries in flight */
};

/* public interfaces */
static void dnsbls_destroy(void);

static bool dnsbls_start(struct auth_client *);
static inline void dnsbls_generic_cancel(struct auth_client *, const char *);
static void dnsbls_timeout(struct auth_client *);
static void dnsbls_cancel(struct auth_client *);
static void dnsbls_cancel_none(struct auth_client *);

/* private interfaces */
static void unref_dnsbl(struct dnsbl *);
static struct dnsbl *new_dnsbl(const char *, const char *, uint8_t, rb_dlink_list *);
static struct dnsbl *find_dnsbl(const char *);
static bool dnsbl_check_reply(struct dnsbl_lookup *, const char *);
static void dnsbl_dns_callback(const char *, bool, query_type, void *);
static void initiate_dnsbl_dnsquery(struct dnsbl *, struct auth_client *);

/* Variables */
static rb_dlink_list dnsbl_list = { NULL, NULL, 0 };
static int dnsbl_timeout = DNSBL_TIMEOUT_DEFAULT;

/* private interfaces */

static void
unref_dnsbl(struct dnsbl *bl)
{
	rb_dlink_node *ptr, *nptr;

	bl->refcount--;
	if (bl->delete && bl->refcount <= 0)
	{
		RB_DLINK_FOREACH_SAFE(ptr, nptr, bl->filters.head)
		{
			rb_dlinkDelete(ptr, &bl->filters);
			rb_free(ptr);
		}

		rb_dlinkFindDestroy(bl, &dnsbl_list);
		rb_free(bl);
	}
}

static struct dnsbl *
new_dnsbl(const char *name, const char *reason, uint8_t iptype, rb_dlink_list *filters)
{
	struct dnsbl *bl;

	if (name == NULL || reason == NULL || iptype == 0)
		return NULL;

	if((bl = find_dnsbl(name)) == NULL)
	{
		bl = rb_malloc(sizeof(struct dnsbl));
		rb_dlinkAddAlloc(bl, &dnsbl_list);
	}
	else
		bl->delete = false;

	rb_strlcpy(bl->host, name, IRCD_RES_HOSTLEN + 1);
	rb_strlcpy(bl->reason, reason, BUFSIZE);
	bl->iptype = iptype;

	rb_dlinkMoveList(filters, &bl->filters);

	bl->lastwarning = 0;

	return bl;
}

static struct dnsbl *
find_dnsbl(const char *name)
{
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, dnsbl_list.head)
	{
		struct dnsbl *bl = (struct dnsbl *)ptr->data;

		if (!strcasecmp(bl->host, name))
			return bl;
	}

	return NULL;
}

static inline bool
dnsbl_check_reply(struct dnsbl_lookup *bllookup, const char *ipaddr)
{
	struct dnsbl *bl = bllookup->bl;
	const char *lastoctet;
	rb_dlink_node *ptr;

	/* No filters and entry found - thus positive match */
	if (!rb_dlink_list_length(&bl->filters))
		return true;

	/* Below will prolly have to change if IPv6 address replies are sent back */
	if ((lastoctet = strrchr(ipaddr, '.')) == NULL || *(++lastoctet) == '\0')
		goto blwarn;

	RB_DLINK_FOREACH(ptr, bl->filters.head)
	{
		struct dnsbl_filter *filter = ptr->data;
		const char *cmpstr;

		if (filter->type == FILTER_ALL)
			cmpstr = ipaddr;
		else if (filter->type == FILTER_LAST)
			cmpstr = lastoctet;
		else
		{
			warn_opers(L_CRIT, "dnsbl: Unknown dnsbl filter type (host %s): %d",
					bl->host, filter->type);
			exit(EX_PROVIDER_ERROR);
		}

		if (strcmp(cmpstr, filter->filter) == 0)
			/* Match! */
			return true;
	}

	return false;
blwarn:
	if (bl->lastwarning + 3600 < rb_current_time())
	{
		warn_opers(L_WARN, "Garbage/undecipherable reply received from dnsbl %s (reply %s)",
				bl->host, ipaddr);
		bl->lastwarning = rb_current_time();
	}

	return false;
}

static void
dnsbl_dns_callback(const char *result, bool status, query_type type, void *data)
{
	struct dnsbl_lookup *bllookup = (struct dnsbl_lookup *)data;
	struct dnsbl_user *bluser;
	struct dnsbl *bl;
	struct auth_client *auth;

	lrb_assert(bllookup != NULL);
	lrb_assert(bllookup->auth != NULL);

	bl = bllookup->bl;
	auth = bllookup->auth;

	if((bluser = get_provider_data(auth, SELF_PID)) == NULL)
		return;

	if (result != NULL && status && dnsbl_check_reply(bllookup, result))
	{
		/* Match found, so proceed no further */
		bl->hits++;
		reject_client(auth, SELF_PID, bl->host, bl->reason);
		dnsbls_cancel(auth);
		return;
	}

	unref_dnsbl(bl);
	cancel_query(bllookup->query);	/* Ignore future responses */
	rb_dlinkDelete(&bllookup->node, &bluser->queries);
	rb_free(bllookup);

	if(!rb_dlink_list_length(&bluser->queries))
	{
		/* Done here */
		notice_client(auth->cid, "*** No DNSBL entry found for this IP");
		rb_free(bluser);
		set_provider_data(auth, SELF_PID, NULL);
		set_provider_timeout_absolute(auth, SELF_PID, 0);
		provider_done(auth, SELF_PID);

		auth_client_unref(auth);
	}
}

static void
initiate_dnsbl_dnsquery(struct dnsbl *bl, struct auth_client *auth)
{
	struct dnsbl_lookup *bllookup = rb_malloc(sizeof(struct dnsbl_lookup));
	struct dnsbl_user *bluser = get_provider_data(auth, SELF_PID);
	char buf[IRCD_RES_HOSTLEN + 1];
	int aftype;

	bllookup->bl = bl;
	bllookup->auth = auth;

	aftype = GET_SS_FAMILY(&auth->c_addr);
	if((aftype == AF_INET && (bl->iptype & IPTYPE_IPV4) == 0) ||
		(aftype == AF_INET6 && (bl->iptype & IPTYPE_IPV6) == 0))
		/* Incorrect dnsbl type for this IP... */
	{
		rb_free(bllookup);
		return;
	}

	build_rdns(buf, sizeof(buf), &auth->c_addr, bl->host);
	bllookup->query = lookup_ip(buf, AF_INET, dnsbl_dns_callback, bllookup);

	rb_dlinkAdd(bllookup, &bllookup->node, &bluser->queries);
	bl->refcount++;
}

static inline bool
lookup_all_dnsbls(struct auth_client *auth)
{
	struct dnsbl_user *bluser = get_provider_data(auth, SELF_PID);
	rb_dlink_node *ptr;
	int iptype;

	if(GET_SS_FAMILY(&auth->c_addr) == AF_INET)
		iptype = IPTYPE_IPV4;
	else if(GET_SS_FAMILY(&auth->c_addr) == AF_INET6)
		iptype = IPTYPE_IPV6;
	else
		return false;

	bluser->started = true;
	notice_client(auth->cid, "*** Checking your IP against DNSBLs");

	RB_DLINK_FOREACH(ptr, dnsbl_list.head)
	{
		struct dnsbl *bl = (struct dnsbl *)ptr->data;

		if (!bl->delete && (bl->iptype & iptype))
			initiate_dnsbl_dnsquery(bl, auth);
	}

	if(!rb_dlink_list_length(&bluser->queries))
		/* None checked. */
		return false;

	set_provider_timeout_relative(auth, SELF_PID, dnsbl_timeout);

	return true;
}

static inline void
delete_dnsbl(struct dnsbl *bl)
{
	if (bl->refcount > 0)
		bl->delete = true;
	else
	{
		rb_dlinkFindDestroy(bl, &dnsbl_list);
		rb_free(bl);
	}
}

static void
delete_all_dnsbls(void)
{
	rb_dlink_node *ptr, *nptr;

	RB_DLINK_FOREACH_SAFE(ptr, nptr, dnsbl_list.head)
	{
		delete_dnsbl(ptr->data);
	}
}

/* public interfaces */
static bool
dnsbls_start(struct auth_client *auth)
{
	lrb_assert(get_provider_data(auth, SELF_PID) == NULL);

	if (!rb_dlink_list_length(&dnsbl_list)) {
		/* Nothing to do... */
		provider_done(auth, SELF_PID);
		return true;
	}

	auth_client_ref(auth);

	set_provider_data(auth, SELF_PID, rb_malloc(sizeof(struct dnsbl_user)));

	if (run_after_provider(auth, "rdns") && run_after_provider(auth, "ident")) {
		/* Start the lookup if ident and rdns are finished, or not loaded. */
		if (!lookup_all_dnsbls(auth)) {
			dnsbls_cancel_none(auth);
			return true;
		}
	}

	return true;
}

/* This is called every time a provider is completed as long as we are marked not done */
static void
dnsbls_initiate(struct auth_client *auth, uint32_t provider)
{
	struct dnsbl_user *bluser = get_provider_data(auth, SELF_PID);

	lrb_assert(provider != SELF_PID);
	lrb_assert(!is_provider_done(auth, SELF_PID));
	lrb_assert(rb_dlink_list_length(&dnsbl_list) > 0);

	if (bluser == NULL || bluser->started) {
		/* Nothing to do */
		return;
	} else if (run_after_provider(auth, "rdns") && run_after_provider(auth, "ident")) {
		/* Start the lookup if ident and rdns are finished, or not loaded. */
		if (!lookup_all_dnsbls(auth)) {
			dnsbls_cancel_none(auth);
		}
	}
}

static inline void
dnsbls_generic_cancel(struct auth_client *auth, const char *message)
{
	rb_dlink_node *ptr, *nptr;
	struct dnsbl_user *bluser = get_provider_data(auth, SELF_PID);

	if(bluser == NULL)
		return;

	if(rb_dlink_list_length(&bluser->queries))
	{
		notice_client(auth->cid, message);

		RB_DLINK_FOREACH_SAFE(ptr, nptr, bluser->queries.head)
		{
			struct dnsbl_lookup *bllookup = ptr->data;

			cancel_query(bllookup->query);
			unref_dnsbl(bllookup->bl);

			rb_dlinkDelete(&bllookup->node, &bluser->queries);
			rb_free(bllookup);
		}
	}

	rb_free(bluser);
	set_provider_data(auth, SELF_PID, NULL);
	set_provider_timeout_absolute(auth, SELF_PID, 0);
	provider_done(auth, SELF_PID);

	auth_client_unref(auth);
}

static void
dnsbls_timeout(struct auth_client *auth)
{
	dnsbls_generic_cancel(auth, "*** No response from DNSBLs");
}

static void
dnsbls_cancel(struct auth_client *auth)
{
	dnsbls_generic_cancel(auth, "*** Aborting DNSBL checks");
}

static void
dnsbls_cancel_none(struct auth_client *auth)
{
	dnsbls_generic_cancel(auth, "*** Could not check DNSBLs");
}

static void
dnsbls_destroy(void)
{
	rb_dictionary_iter iter;
	struct auth_client *auth;

	RB_DICTIONARY_FOREACH(auth, &iter, auth_clients)
	{
		dnsbls_cancel(auth);
		/* auth is now invalid as we have no reference */
	}

	delete_all_dnsbls();
}

static void
add_conf_dnsbl(const char *key, int parc, const char **parv)
{
	rb_dlink_list filters = { NULL, NULL, 0 };
	char *tmp, *elemlist = rb_strdup(parv[2]);
	uint8_t iptype;

	if(*elemlist == '*')
		goto end;

	for(char *elem = rb_strtok_r(elemlist, ",", &tmp); elem; elem = rb_strtok_r(NULL, ",", &tmp))
	{
		struct dnsbl_filter *filter = rb_malloc(sizeof(struct dnsbl_filter));
		int dot_c = 0;
		filter_t type = FILTER_LAST;

		/* Check dnsbl filter type and for validity */
		for(char *c = elem; *c != '\0'; c++)
		{
			if(*c == '.')
			{
				if(++dot_c > 3)
				{
					warn_opers(L_CRIT, "dnsbl: addr_conf_dnsbl got a bad filter (too many octets)");
					exit(EX_PROVIDER_ERROR);
				}

				type = FILTER_ALL;
			}
			else if(!isdigit(*c))
			{
				warn_opers(L_CRIT, "dnsbl: addr_conf_dnsbl got a bad filter (invalid character in dnsbl filter: %c)",
						*c);
				exit(EX_PROVIDER_ERROR);
			}
		}

		if(dot_c > 0 && dot_c < 3)
		{
			warn_opers(L_CRIT, "dnsbl: addr_conf_dnsbl got a bad filter (insufficient octets)");
			exit(EX_PROVIDER_ERROR);
		}

		filter->type = type;
		rb_strlcpy(filter->filter, elem, sizeof(filter->filter));
		rb_dlinkAdd(filter, &filter->node, &filters);
	}

end:
	rb_free(elemlist);

	iptype = atoi(parv[1]) & 0x3;
	if(new_dnsbl(parv[0], parv[3], iptype, &filters) == NULL)
	{
		warn_opers(L_CRIT, "dnsbl: addr_conf_dnsbl got a malformed dnsbl");
		exit(EX_PROVIDER_ERROR);
	}
}

static void
del_conf_dnsbl(const char *key, int parc, const char **parv)
{
	struct dnsbl *bl = find_dnsbl(parv[0]);
	if(bl == NULL)
	{
		/* Not fatal for now... */
		warn_opers(L_WARN, "dnsbl: tried to remove nonexistent dnsbl %s", parv[0]);
		return;
	}

	delete_dnsbl(bl);
}

static void
del_conf_dnsbl_all(const char *key, int parc, const char **parv)
{
	delete_all_dnsbls();
}

static void
add_conf_dnsbl_timeout(const char *key, int parc, const char **parv)
{
	int timeout = atoi(parv[0]);

	if(timeout < 0)
	{
		warn_opers(L_CRIT, "dnsbl: dnsbl timeout < 0 (value: %d)", timeout);
		exit(EX_PROVIDER_ERROR);
	}

	dnsbl_timeout = timeout;
}

#if 0
static void
dnsbl_stats(uint32_t rid, char letter)
{
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, dnsbl_list.head)
	{
		struct dnsbl *bl = ptr->data;

		if(bl->delete)
			continue;

		stats_result(rid, letter, "%s %hhu %u", bl->host, bl->iptype, bl->hits);
	}

	stats_done(rid, letter);
}
#endif

struct auth_opts_handler dnsbl_options[] =
{
	{ "rbl", 4, add_conf_dnsbl },
	{ "rbl_del", 1, del_conf_dnsbl },
	{ "rbl_del_all", 0, del_conf_dnsbl_all },
	{ "rbl_timeout", 1, add_conf_dnsbl_timeout },
	{ NULL, 0, NULL },
};

struct auth_provider dnsbl_provider =
{
	.name = "dnsbl",
	.letter = 'B',
	.destroy = dnsbls_destroy,
	.start = dnsbls_start,
	.cancel = dnsbls_cancel,
	.timeout = dnsbls_timeout,
	.completed = dnsbls_initiate,
	.opt_handlers = dnsbl_options,
	/* .stats_handler = { 'B', dnsbl_stats }, */
};
