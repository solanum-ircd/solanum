/*
 * Oper name extban type: matches oper names
 * -- jilles
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "privilege.h"
#include "s_newconf.h"
#include "ircd.h"

static const char extb_desc[] = "Oper name ($o) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_opername(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV2(extb_opername, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	extban_table['o'] = eb_opername;

	return 0;
}

static void
_moddeinit(void)
{
	extban_table['o'] = NULL;
}

static int eb_opername(const char *data, struct Client *client_p,
		struct Channel *chptr, long mode_type)
{

	if (data != NULL)
        return match(client_p->user->opername, data)? EXTBAN_MATCH : EXTBAN_NOMATCH;
	return IsOper(client_p) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}

