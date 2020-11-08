/*
 * Treat cmode +-O as +-iI $o.
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "chmode.h"

static const char chm_operonly_compat[] =
	"Adds an emulated channel mode +O which is converted into mode +i and +I $o";

static int _modinit(void);
static void _moddeinit(void);
static ChannelModeFunc chm_operonly;

DECLARE_MODULE_AV2(chm_operonly_compat, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, chm_operonly_compat);

static int
_modinit(void)
{
	chmode_table['O'] = (struct ChannelMode){chm_operonly, 0, 0};
	return 0;
}

static void
_moddeinit(void)
{
	chmode_table['O'] = (struct ChannelMode){chm_nosuch, 0, 0};
}

static void
chm_operonly(struct Client *source_p, struct Channel *chptr,
	int alevel, const char *arg, int *errors, int dir, char c, long mode_type)
{
	if (MyClient(source_p)) {
		chm_simple(source_p, chptr, alevel, NULL,
				errors, dir, 'i', MODE_INVITEONLY);
		chm_ban(source_p, chptr, alevel, "$o",
				errors, dir, 'I', CHFL_INVEX);
	} else
		chm_nosuch(source_p, chptr, alevel, NULL,
				errors, dir, c, mode_type);
}
