/*
 * Treat cmode +-R as +-q $~a.
 * -- jilles
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "chmode.h"

static const char chm_quietunreg_compat_desc[] =
	"Adds an emulated channel mode +R which is converted into mode +q $~a";

static int _modinit(void);
static void _moddeinit(void);
static ChannelModeFunc chm_quietunreg;

DECLARE_MODULE_AV2(chm_quietunreg_compat, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, chm_quietunreg_compat_desc);

static int
_modinit(void)
{
	chmode_table['R'] = (struct ChannelMode){ chm_quietunreg, 0, 0 };
	return 0;
}

static void
_moddeinit(void)
{
	chmode_table['R'] = (struct ChannelMode){ chm_nosuch, 0, 0 };
}

static void
chm_quietunreg(struct Client *source_p, struct Channel *chptr,
	int alevel, const char *arg, int *errors, int dir, char c, long mode_type)
{
	if (MyClient(source_p))
		chm_ban(source_p, chptr, alevel, "$~a",
				errors, dir, 'q', CHFL_QUIET);
	else
		chm_nosuch(source_p, chptr, alevel, NULL,
				errors, dir, c, mode_type);
}
