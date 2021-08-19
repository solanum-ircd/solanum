/*
 * Override WHOIS logic to hide channel memberships that are not common.
 *   -- kaniini
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_newconf.h"

static const char hide_desc[] = "Hides channel memberships not shared";

static void h_huc_doing_whois_channel_visibility(void *);

mapi_hfn_list_av1 huc_hfnlist[] = {
	{ "doing_whois_channel_visibility", h_huc_doing_whois_channel_visibility },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(hide_uncommon_channels, NULL, NULL, NULL, NULL, huc_hfnlist, NULL, NULL, hide_desc);

static void
h_huc_doing_whois_channel_visibility(void *data_)
{
	hook_data_channel_visibility *data = data_;
	data->approved = data->approved && (!IsInvisible(data->target) || data->clientms != NULL);
}
