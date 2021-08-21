/*
 * This module restricts channel creation to opered up users
 * only. This module could be useful for running private chat
 * systems, or if a network gets droneflood problems. It will
 * return ERR_NEEDREGGEDNICK on failure.
 *    -- nenolod
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "snomask.h"
#include "numeric.h"
#include "s_newconf.h"

static const char restrict_desc[] = "Restricts channel creation to IRC operators";

static void h_can_create_channel_authenticated(void *);

mapi_hfn_list_av1 restrict_hfnlist[] = {
	{ "can_create_channel", h_can_create_channel_authenticated },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(createoperonly, NULL, NULL, NULL, NULL, restrict_hfnlist, NULL, NULL, restrict_desc);

static void
h_can_create_channel_authenticated(void *data_)
{
	hook_data_client_approval *data = data_;
	struct Client *source_p = data->client;

	if (!IsOperGeneral(source_p))
	{
		sendto_one_notice(source_p, ":*** Channel creation is restricted to network staff only.");
		data->approved = ERR_NEEDREGGEDNICK;
	}
}
