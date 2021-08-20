/*
 * invex_regonly.c Allow invite exemptions to bypass registered-only (+r)
 */
#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "channel.h"
#include "s_conf.h"
#include "numeric.h"

static void h_can_join(void *);

mapi_hfn_list_av1 invex_regonly_hfnlist[] = {
	{ "can_join", h_can_join },
	{ NULL, NULL }
};

DECLARE_MODULE_AV1(invex_regonly, NULL, NULL, NULL, NULL, invex_regonly_hfnlist, "$Revision$");

static void
h_can_join(void *data_)
{
	hook_data_channel *data = data_;
	struct Client *source_p = data->client;
	struct Channel *chptr = data->chptr;
	struct Ban *invex = NULL;
	struct matchset ms;
	rb_dlink_node *ptr;
	
	if(data->approved != ERR_NEEDREGGEDNICK)
		return;
	if(!ConfigChannel.use_invex)
		return;

	matchset_for_client(source_p, &ms);

	RB_DLINK_FOREACH(ptr, chptr->invexlist.head)
	{
		invex = ptr->data;
		if (matches_mask(&ms, invex->banstr) ||
				match_extban(invex->banstr, source_p, chptr, CHFL_INVEX))
		{
			data->approved = 0;
			break;
		}
	}
}
