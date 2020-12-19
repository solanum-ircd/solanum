/*
 *  Copyright 2020 Ed Kellett
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include <stdinc.h>
#include <channel.h>
#include <hook.h>

#include "client_util.h"
#include "ircd_util.h"
#include "tap/basic.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__

static struct Channel *channel;
static struct Client *client;

static hook_data_channel_approval chmode_hdata;

static void
chmode_access_hook(void *data_)
{
	hook_data_channel_approval *data = data_;
	chmode_hdata = *data;
}

void
test_chmode_parse(void)
{
	add_hook_prio("get_channel_access", chmode_access_hook, HOOK_MONITOR);

	set_channel_mode(client, client, channel, NULL, 2, (const char *[]){"o", "foo"});
	is_string("+o foo", chmode_hdata.modestr, MSG);

	set_channel_mode(client, client, channel, NULL, 3, (const char *[]){"o", "foo", "bar"});
	is_string("+o foo", chmode_hdata.modestr, MSG);

	chmode_hdata.modestr = NULL;
	set_channel_mode(client, client, channel, NULL, 3, (const char *[]){"+-=+++--+", "foo", "bar"});
	is_bool(true, chmode_hdata.modestr == NULL, MSG);

	set_channel_mode(client, client, channel, NULL, 1, (const char *[]){"b"});
	is_string("=b", chmode_hdata.modestr, MSG);

	set_channel_mode(client, client, channel, NULL, 2, (const char *[]){"bb", "foo"});
	is_string("+b=b foo", chmode_hdata.modestr, MSG);

	set_channel_mode(client, client, channel, NULL, 1, (const char *[]){"iqiqiqiq"});
	is_string("+i=q+i=q+i=q+i=q", chmode_hdata.modestr, MSG);

	remove_hook("get_channel_access", chmode_access_hook);
}

void
test_chmode_limits(void)
{
	char chmode_buf[2 + MAXMODEPARAMS + 1] = "+";
	const char *chmode_parv[1 + MAXMODEPARAMS + 1] = { chmode_buf };
	add_hook_prio("get_channel_access", chmode_access_hook, HOOK_MONITOR);

	for (size_t i = 0; i < MAXMODEPARAMS + 1; i++)
	{
		chmode_buf[i + 1] = 'l';
		chmode_parv[i + 1] = "7";
	}

	set_channel_mode(client, client, channel, NULL, 1 + MAXMODEPARAMS + 1, chmode_parv);

	is_int('+', chmode_hdata.modestr[0], MSG);

	for (size_t i = 0; i < MAXMODEPARAMS; i++)
	{
		is_int('l', chmode_hdata.modestr[i + 1], MSG);
	}

	is_int(' ', chmode_hdata.modestr[MAXMODEPARAMS + 1], MSG);

	for (size_t i = 0; i < MAXMODEPARAMS; i++)
	{
		is_int(' ', chmode_hdata.modestr[MAXMODEPARAMS + 1 + i * 2], MSG);
		is_int('7', chmode_hdata.modestr[MAXMODEPARAMS + 2 + i * 2], MSG);
	}

	is_int('\0', chmode_hdata.modestr[MAXMODEPARAMS * 3 + 1], MSG);

	remove_hook("get_channel_access", chmode_access_hook);
}

static void
chmode_init(void)
{
	channel = make_channel();
	client = make_local_person();
}

int
main(int argc, char *argv[])
{
	plan_lazy();

	ircd_util_init(__FILE__);
	client_util_init();

	chmode_init();

	test_chmode_parse();
	test_chmode_limits();

	client_util_free();
	ircd_util_free();

	return 0;
}
