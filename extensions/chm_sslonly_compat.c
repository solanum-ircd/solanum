/*
 * Treat cmode +-S as +-b $~z.
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "chmode.h"

static const char chm_sslonly_compat_desc[] =
	"Adds an emulated channel mode +S which is converted into mode +b $~z";

static int _modinit(void);
static void _moddeinit(void);
static ChannelModeFunc chm_sslonly;

DECLARE_MODULE_AV2(chm_sslonly_compat, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, chm_sslonly_compat_desc);

static int
_modinit(void)
{
	chmode_table['S'] = (struct ChannelMode){ chm_sslonly, 0, 0 };
	return 0;
}

static void
_moddeinit(void)
{
	chmode_table['S'] = (struct ChannelMode){ chm_nosuch, 0, 0 };
}

static void
chm_sslonly(struct Client *source_p, struct Channel *chptr,
	int alevel, const char *arg, int *errors, int dir, char c, long mode_type)
{
	if (MyClient(source_p))
		chm_ban(source_p, chptr, alevel, "$~z",
				errors, dir, 'b', CHFL_BAN);
	else
		chm_nosuch(source_p, chptr, alevel, NULL,
				errors, dir, c, mode_type);
}
