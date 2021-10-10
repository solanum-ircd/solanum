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

static const char chm_operonly_desc[] =
	"Adds channel mode +O which makes a channel operator-only";

static void h_can_join(void *);

mapi_hfn_list_av1 operonly_hfnlist[] = {
	{ "can_join", h_can_join },
	{ NULL, NULL }
};

static struct ChannelMode *mymode;

static int
_modinit(void)
{
	mymode = cflag_add('O', chm_simple);
	if (mymode == NULL)
		return -1;
	else
		mymode->priv = "oper:cmodes";

	return 0;
}


static void
_moddeinit(void)
{
	cflag_orphan('O');
}

DECLARE_MODULE_AV2(chm_operonly, _modinit, _moddeinit, NULL, NULL, operonly_hfnlist, NULL, NULL, chm_operonly_desc);

static void
h_can_join(void *data_)
{
	hook_data_channel *data = data_;
	struct Client *source_p = data->client;
	struct Channel *chptr = data->chptr;

	if((chptr->mode.mode & mymode->mode_type) && !IsOper(source_p)) {
		sendto_one_numeric(source_p, 520, "%s :Cannot join channel (+O) - you are not an IRC operator", chptr->chname);
		data->approved = ERR_CUSTOM;
	}
}

