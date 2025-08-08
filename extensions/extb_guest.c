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
	(void)chptr;

	if (data == NULL)
		return EXTBAN_INVALID;

	const char *idx = strchr(data, '#');

	if (idx != NULL && idx[1] == '\0')
		/* Users cannot have empty realnames,
		 * so don't let a ban be set matching one
		 */
		return EXTBAN_INVALID;

	if (!EmptyString(client_p->user->suser))
		return EXTBAN_NOMATCH;

	if (idx != NULL)
	{
		char buf[BUFSIZE];

		// Copy the nick!user@host part of the ban
		memcpy(buf, data, (idx - data));
		buf[(idx - data)] = '\0';

		// Advance to the realname part of the ban
		idx++;

		if (client_matches_mask(client_p, buf) && match(idx, client_p->info))
			return EXTBAN_MATCH;

		return EXTBAN_NOMATCH;
	}

	if (client_matches_mask(client_p, data))
		return EXTBAN_MATCH;

	return EXTBAN_NOMATCH;
}
