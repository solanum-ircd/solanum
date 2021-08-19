#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "s_conf.h"

static void check_new_user(void *data);
mapi_hfn_list_av1 drain_hfnlist[] = {
	{ "new_local_user", (hookfn) check_new_user },
	{ NULL, NULL }
};

static const char drain_desc[] = "Prevents new, non-exempt users from connecting to this server.";

DECLARE_MODULE_AV2(drain, NULL, NULL, NULL, NULL,
			drain_hfnlist, NULL, NULL, drain_desc);


static void
check_new_user(void *vdata)
{
	struct Client *source_p = vdata;
	const char *drain_reason = ConfigFileEntry.drain_reason;

	if (IsAnyDead(source_p))
		return;

	if (drain_reason == NULL)
		drain_reason = "This server is not accepting connections.";

	if (IsExemptKline(source_p))
		return;

	exit_client(source_p, source_p, &me, drain_reason);
}
