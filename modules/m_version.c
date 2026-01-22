/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_version.c: Shows ircd version information.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

#include <stdinc.h>
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_serv.h"
#include "supported.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

static const char version_desc[] =
	"Provides the VERSION command to display server version information";

static char *confopts(void);
static int h_doing_version_confopts;

static void m_version(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void mo_version(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message version_msgtab = {
	"VERSION", 0, 0, 0, 0,
	{mg_unreg, {m_version, 0}, {mo_version, 0}, {mo_version, 0}, mg_ignore, {mo_version, 0}}
};

mapi_clist_av1 version_clist[] = { &version_msgtab, NULL };

mapi_hlist_av1 version_hlist[] = {
	{ "doing_version_confopts", &h_doing_version_confopts },
	{ NULL, NULL },
};

DECLARE_MODULE_AV2(version, NULL, NULL, version_clist, version_hlist, NULL, NULL, NULL, version_desc);

/*
 * m_version - VERSION command handler
 *      parv[1] = remote server
 */
static void
m_version(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0L;

	if(parc > 1)
	{
		if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
		{
			/* safe enough to give this on a local connect only */
			sendto_one(source_p, form_str(RPL_LOAD2HI),
				   me.name, source_p->name, "VERSION");
			return;
		}
		else
			last_used = rb_current_time();

		if(hunt_server(client_p, source_p, ":%s VERSION :%s", 1, parc, parv) != HUNTED_ISME)
			return;
	}

	sendto_one_numeric(source_p, RPL_VERSION, form_str(RPL_VERSION),
			   ircd_version, serno,
#ifdef CUSTOM_BRANDING
			   PACKAGE_NAME "-" PACKAGE_VERSION,
#endif
			   me.name, confopts(), TS_CURRENT);

	show_isupport(source_p);
}

/*
 * mo_version - VERSION command handler
 *      parv[1] = remote server
 */
static void
mo_version(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(hunt_server(client_p, source_p, ":%s VERSION :%s", 1, parc, parv) == HUNTED_ISME)
	{
		sendto_one_numeric(source_p, RPL_VERSION, form_str(RPL_VERSION),
				   ircd_version, serno,
#ifdef CUSTOM_BRANDING
				   PACKAGE_NAME "-" PACKAGE_VERSION,
#endif
				   me.name, confopts(), TS_CURRENT);
		show_isupport(source_p);
	}
}

/* confopts()
 * input  - none
 * output - ircd.conf option string
 * side effects - none
 */
static char *
confopts(void)
{
	static char result[64];
	char *p = result;
	int c;
	char opts[256] = { 0 };

	if(ConfigChannel.use_except)
		opts['e'] = 1;

	if(ConfigChannel.use_invex)
		opts['I'] = 1;

	if(ConfigChannel.use_knock)
		opts['K'] = 1;

	opts['M'] = 1;
	opts['p'] = 1;

	if (opers_see_all_users || ConfigFileEntry.operspy_dont_care_user_info)
		opts['S'] = 1;

#ifdef HAVE_LIBZ
	opts['Z'] = 1;
#endif

	opts['6'] = 1;

	call_hook(h_doing_version_confopts, opts);

	for (c = 'A'; c <= 'Z'; c++)
	{
		if (opts[c])
			*p++ = (char)c;

		if (opts[tolower(c)])
			*p++ = tolower(c);
	}

	for (c = '0'; c <= '9'; c++)
	{
		if (opts[c])
			*p++ = (char)c;
	}

	*p = '\0';
	return result;
}
