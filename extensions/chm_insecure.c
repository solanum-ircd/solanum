#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"
#include "chmode.h"

static const char chm_insecure_desc[] =
	"Adds channel mode +U that allows non-SSL users to join a channel, "
	"disallowing them by default";

static void h_can_join(void *);

mapi_hfn_list_av1 sslonly_hfnlist[] = {
	{ "can_join", h_can_join },
	{ NULL, NULL }
};

static struct ChannelMode *mymode;

static int
_modinit(void)
{
	mymode = cflag_add('U', chm_simple);
	if (mymode == NULL)
		return -1;

	return 0;
}


static void
_moddeinit(void)
{
	cflag_orphan('U');
}

DECLARE_MODULE_AV2(chm_insecure, _modinit, _moddeinit, NULL, NULL, sslonly_hfnlist, NULL, NULL, chm_insecure_desc);

static void
h_can_join(void *data_)
{
	hook_data_channel *data = data_;
	struct Client *source_p = data->client;
	struct Channel *chptr = data->chptr;

	if(!(chptr->mode.mode & mymode->mode_type) && !IsSecureClient(source_p)) {
		/* XXX This is equal to ERR_THROTTLE */
		sendto_one_numeric(source_p, 480, "%s :Cannot join channel (-U) - SSL/TLS required", chptr->chname);
		data->approved = ERR_CUSTOM;
	}
}

