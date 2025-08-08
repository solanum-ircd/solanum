/*
 * Account extban type: bans all users with any/matching account
 * -- jilles
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"
#include "supported.h"

static const char extb_desc[] = "Account ($a) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_account(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV2(extb_account, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	extban_table['a'] = eb_account;
	add_isupport("ACCOUNTEXTBAN", isupport_string, "a");

	return 0;
}

static void
_moddeinit(void)
{
	extban_table['a'] = NULL;
	delete_isupport("ACCOUNTEXTBAN");
}

static int eb_account(const char *data, struct Client *client_p,
		struct Channel *chptr, long mode_type)
{

	(void)chptr;
	/* $a alone matches any logged in user */
	if (data == NULL)
		return EmptyString(client_p->user->suser) ? EXTBAN_NOMATCH : EXTBAN_MATCH;
	/* $a:MASK matches users logged in under matching account */
	return match(data, client_p->user->suser) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}
