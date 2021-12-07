#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "numeric.h"
#include "s_user.h"
#include "s_conf.h"

static const char override_kick_immunity_desc[] =
	"Prevents +p users (oper override) from being kicked from any channel";

static void can_kick(void *data);

mapi_hfn_list_av1 override_kick_immunity_hfnlist[] = {
	{ "can_kick", can_kick, HOOK_HIGHEST },
	{ NULL, NULL }
};

static void
can_kick(void *vdata)
{
	hook_data_channel_approval *data = vdata;

	if (data->target->umodes & user_modes['p'])
	{
		if (data->client->umodes & user_modes['p'])
		{
			/* Using oper-override to kick an oper
			 * who's also using oper-override, better
			 * report what happened.
			 */
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s is using oper-override on %s (KICK %s)",
					get_oper_name(data->client), data->chptr->chname, data->target->name);
		}
		else
		{
			/* Like cmode +M, let's report any attempt
			 * to kick the immune oper.
			 */
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s attempted to kick %s from %s (who is +p)",
				data->client->name, data->target->name, data->chptr->chname);
			sendto_one_numeric(data->client, ERR_ISCHANSERVICE, "%s %s :Cannot kick immune IRC operators.",
				data->target->name, data->chptr->chname);
			data->approved = 0;
		}
		return;
	}
}

DECLARE_MODULE_AV2(override_kick_immunity, NULL, NULL, NULL, NULL,
			override_kick_immunity_hfnlist, NULL, NULL, override_kick_immunity_desc);
