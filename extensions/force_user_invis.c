/*
 * Deny user to remove +i flag except they are irc operators
 *
 * Based off no_oper_invis.c by jilles
 *
 * Note that +i must be included in default_umodes
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_newconf.h"

static const char noi_desc[] =
	"Do not allow users to remove user mode +i unless they are operators";

static void h_noi_umode_changed(void *);

mapi_hfn_list_av1 noi_hfnlist[] = {
	{ "umode_changed", h_noi_umode_changed },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(force_user_invis, NULL, NULL, NULL, NULL, noi_hfnlist, NULL, NULL, noi_desc);

static void
h_noi_umode_changed(void *data)
{
	hook_data_umode_changed *hdata = data;
	struct Client *source_p = hdata->client;

	if (MyClient(source_p) && !IsOperGeneral(source_p) && !IsInvisible(source_p)) {
		SetInvisible(source_p);
	}
}
