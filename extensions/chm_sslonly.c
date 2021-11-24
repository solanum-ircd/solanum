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

static const char chm_sslonly_desc[] =
	"Adds channel mode +S that bans non-SSL users from joing a channel";

static void h_can_join(void *);

mapi_hfn_list_av1 sslonly_hfnlist[] = {
	{ "can_join", h_can_join },
	{ NULL, NULL }
};

static struct ChannelMode *mymode;

static int
_modinit(void)
{
	mymode = cflag_add('S', chm_simple);
	if (mymode == NULL)
		return -1;

	return 0;
}

static void
_moddeinit(void)
{
	cflag_orphan('S');
}

DECLARE_MODULE_AV2(chm_sslonly, _modinit, _moddeinit, NULL, NULL, sslonly_hfnlist, NULL, NULL, chm_sslonly_desc);

static void
h_can_join(void *data_)
{
	hook_data_channel *data = data_;
	struct Client *source_p = data->client;
	struct Channel *chptr = data->chptr;

	if((chptr->mode.mode & mymode->mode_type) && !IsSecureClient(source_p)) {
		/* XXX This is equal to ERR_THROTTLE */
		sendto_one_numeric(source_p, 480, "%s :Cannot join channel (+S) - SSL/TLS required", chptr->chname);
		data->approved = ERR_CUSTOM;
	}
}

