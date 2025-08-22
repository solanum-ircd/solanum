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

static const char chm_serviceonly_desc[] =
	"Adds channel mode +X which makes a channel service-only";

static void h_can_join(void *);

mapi_hfn_list_av1 serviceonly_hfnlist[] = {
	{ "can_join", h_can_join },
	{ NULL, NULL }
};

static unsigned int mymode;

static int
_modinit(void)
{
	mymode = cflag_add('X', chm_staff);
	if (mymode == 0)
		return -1;

	return 0;
}


static void
_moddeinit(void)
{
	cflag_orphan('X');
}

DECLARE_MODULE_AV2(chm_serviceonly, _modinit, _moddeinit, NULL, NULL, serviceonly_hfnlist, NULL, NULL, chm_serviceonly_desc);

static void
h_can_join(void *data_)
{
	hook_data_channel *data = data_;
	struct Client *source_p = data->client;
	struct Channel *chptr = data->chptr;

	if((chptr->mode.mode & mymode) && !IsService(source_p)) {
		sendto_one_numeric(source_p, 520, "%s :Cannot join channel (+X) - you are not a service", chptr->chname);
		data->approved = ERR_CUSTOM;
	}
}

