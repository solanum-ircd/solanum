/*
 * Hostmask extban type: bans all users matching a given hostmask, used for stacked extbans
 * -- kaniini
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"

static const char extb_desc[] = "Hostmask ($m) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_hostmask(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV2(extb_hostmask, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	extban_table['m'] = eb_hostmask;
	return 0;
}

static void
_moddeinit(void)
{
	extban_table['m'] = NULL;
}

static int
eb_hostmask(const char *banstr, struct Client *client_p, struct Channel *chptr, long mode_type)
{
	if (banstr == NULL)
		return EXTBAN_INVALID;
	return client_matches_mask(client_p, banstr) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}
