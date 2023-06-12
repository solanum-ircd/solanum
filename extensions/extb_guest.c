/*
 * Guest extban type: bans unidentified users matching nick!user@host.
 * -- TheDaemoness
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"

static const char extb_desc[] = "Guest ($g) extban type - bans unidentified users matching nick!user@host";

static int _modinit(void);
static void _moddeinit(void);
static int eb_guest(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV2(extb_guest, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	extban_table['g'] = eb_guest;

	return 0;
}

static void
_moddeinit(void)
{
	extban_table['g'] = NULL;
}

static int eb_guest(const char *data, struct Client *client_p,
		struct Channel *chptr, long mode_type)
{
	if (data == NULL)
		return EXTBAN_INVALID;

	if (!EmptyString(client_p->user->suser))
		return EXTBAN_NOMATCH;

	return client_matches_mask(client_p, data) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}
