/*
 * m_invex_regonly.c Allow invite exemptions to bypass registered-only (+r)
 */
#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "channel.h"
#include "s_conf.h"
#include "numeric.h"

static void h_can_join(hook_data_channel *);

mapi_hfn_list_av1 invex_regonly_hfnlist[] = {
	{ "can_join", (hookfn) h_can_join },
	{ NULL, NULL }
};

DECLARE_MODULE_AV1(invex_regonly, NULL, NULL, NULL, NULL, invex_regonly_hfnlist, "$Revision$");

static void
h_can_join(hook_data_channel *data)
{
	struct Client *source_p = data->client;
	struct Channel *chptr = data->chptr;
	struct Ban *invex = NULL;
	rb_dlink_node *ptr;
	char src_host[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_iphost[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_althost[NICKLEN + USERLEN + HOSTLEN + 6];

	int use_althost = 0;


	if(data->approved == ERR_NEEDREGGEDNICK) {
		if(!ConfigChannel.use_invex)
				return;

		sprintf(src_host, "%s!%s@%s", source_p->name, source_p->username, source_p->host);
		sprintf(src_iphost, "%s!%s@%s", source_p->name, source_p->username, source_p->sockhost);
		if(source_p->localClient->mangledhost != NULL)
		{
			/* if host mangling mode enabled, also check their real host */
			if(!strcmp(source_p->host, source_p->localClient->mangledhost))
			{
				sprintf(src_althost, "%s!%s@%s", source_p->name, source_p->username, source_p->orighost);
				use_althost = 1;
			}
			/* if host mangling mode not enabled and no other spoof,
		  	* also check the mangled form of their host */
			else if (!IsDynSpoof(source_p))
			{
				sprintf(src_althost, "%s!%s@%s", source_p->name, source_p->username, source_p->localClient->mangledhost);
				use_althost = 1;
			}
		}

		RB_DLINK_FOREACH(ptr, chptr->invexlist.head)
		{
			invex = ptr->data;
			if(match(invex->banstr, src_host)
			   || match(invex->banstr, src_iphost)
			   || match_cidr(invex->banstr, src_iphost)
			   || match_extban(invex->banstr, source_p, chptr, CHFL_INVEX)
			   || (use_althost && match(invex->banstr, src_althost)))
				break;
		}
		if(ptr != NULL)
			data->approved=0;
	}

}

