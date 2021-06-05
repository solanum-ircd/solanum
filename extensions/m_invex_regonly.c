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
	struct matchset ms;
	rb_dlink_node *ptr;
	
	matchset_for_client(source_p, &ms);

	if(data->approved == ERR_NEEDREGGEDNICK) {
		if(!ConfigChannel.use_invex)
				return;

		RB_DLINK_FOREACH(ptr, chptr->invexlist.head)
		{
		invex = ptr->data;
		if (matches_mask(&ms, invex->banstr) ||
				match_extban(invex->banstr, source_p, chptr, CHFL_INVEX))
					break;
		}
		if(ptr != NULL)
			data->approved=0;
	}
}

