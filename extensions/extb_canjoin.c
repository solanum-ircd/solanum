/*
 * Canjoin extban type: matches users who are or are not banned from a
 * specified channel.
 *    -- nenolod/jilles
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "channel.h"
#include "hash.h"
#include "ircd.h"
#include "match.h"

static const char extb_desc[] = "Can join ($j) extban type - matches users who are or are not banned from a specified channel";

static int _modinit(void);
static void _moddeinit(void);
static int eb_canjoin(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV2(extb_canjoin, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	extban_table['j'] = eb_canjoin;

	return 0;
}

static void
_moddeinit(void)
{
	extban_table['j'] = NULL;
}

static int eb_canjoin(const char *data, struct Client *client_p,
		struct Channel *chptr, long mode_type)
{
	struct Channel *chptr2;
	int ret;
	static int recurse = 0;

	(void)mode_type;
	/* don't process a $j in a $j'ed list */
	if (recurse)
		return EXTBAN_INVALID;
	if (data == NULL)
		return EXTBAN_INVALID;
	if (mode_type == CHFL_EXCEPTION)
		return EXTBAN_INVALID;
	chptr2 = find_channel(data);
	/* must exist, and no point doing this with the same channel */
	if (chptr2 == NULL || chptr2 == chptr)
		return EXTBAN_INVALID;
	/* require consistent target */
	if (chptr->chname[0] == '#' && data[0] == '&')
		return EXTBAN_INVALID;
	/* this allows getting some information about ban exceptions
	 * but +s/+p doesn't seem the right criterion */
#if 0
	/* privacy! don't allow +s/+p channels to influence another channel */
	if (!PubChannel(chptr2))
		return EXTBAN_INVALID;
#endif
	recurse = 1;
	/* Check both bans and quiets in the target channel for $j extban */
	const char *forward = NULL;
	struct matchset ms;
	matchset_for_client(client_p, &ms);
	int banned_result = is_banned(chptr2, client_p, NULL, &ms, &forward);
	if (banned_result != 0)
	{
		/* If the ban has a forward, check if user would be banned in the forward channel */
		if (forward != NULL)
		{
			struct Channel *forward_chptr = find_channel(forward);
			if (forward_chptr != NULL && forward_chptr != chptr && forward_chptr != chptr2)
			{
				/* Check if user is banned in forward channel (but don't recurse $j checks) */
				recurse = 0;
				int forward_banned = is_banned(forward_chptr, client_p, NULL, &ms, NULL) != 0 ||
				                     is_quieted(forward_chptr, client_p, NULL, &ms) == CHFL_BAN;
				recurse = 1;
				ret = forward_banned ? EXTBAN_MATCH : EXTBAN_NOMATCH;
			}
			else
			{
				ret = EXTBAN_MATCH;
			}
		}
		else
		{
			ret = EXTBAN_MATCH;
		}
	}
	else
	{
		/* Check quiets separately to also check for forward */
		const char *qforward = NULL;
		int quieted_result = is_quieted(chptr2, client_p, NULL, &ms);
		if (quieted_result == CHFL_BAN)
		{
			/* Quiets don't have forwards, so just match */
			ret = EXTBAN_MATCH;
		}
		else
		{
			ret = EXTBAN_NOMATCH;
		}
	}
	recurse = 0;
	return ret;
}
