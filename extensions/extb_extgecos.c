/*
 * Extended extban type: bans all users with matching nick!user@host#gecos.
 * Requested by Lockwood.
 *  - nenolod
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"

static const char extb_desc[] = "Extended mask ($x) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_extended(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV2(extb_extended, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	extban_table['x'] = eb_extended;

	return 0;
}

static void
_moddeinit(void)
{
	extban_table['x'] = NULL;
}

static int eb_extended(const char *data, struct Client *client_p,
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

	char buf[BUFSIZE];

	if (idx != NULL)
	{
		// Copy the nick!user@host part of the ban
		memcpy(buf, data, (idx - data));
		buf[(idx - data)] = '\0';

		// Advance to the realname part of the ban
		idx++;

		if (client_matches_mask(client_p, buf) && match(idx, client_p->info))
			return EXTBAN_MATCH;
	}
	else
	{
		// Treat data as a pattern to match against the full nick!user@host#gecos.
		snprintf(buf, sizeof buf, "%s!%s@%s#%s",
			client_p->name, client_p->username, client_p->host, client_p->info);

		if (match(data, buf))
			return EXTBAN_MATCH;
	}

	return EXTBAN_NOMATCH;
}
