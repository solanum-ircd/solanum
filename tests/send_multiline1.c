/*
 *  Copyright (C) 2020 Ed Kellett
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "tap/basic.h"

#include "ircd_util.h"
#include "client_util.h"

#include "send.h"
#include "s_serv.h"
#include "monitor.h"
#include "s_conf.h"
#include "hash.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__

static void sendto_multiline_basic(void)
{
	struct Client *user = make_local_person();
	const char *s;

	/* multiline with no items should do nothing */
	send_multiline_init(user, " ", "foo");
	send_multiline_fini(user, NULL);
	is_client_sendq_empty(user, MSG);

	/* 510 = 17 * 30. line the end of an item with the end of the 510-byte data exactly */
	send_multiline_init(user, " ", "prefix78901234567 ");
	for (size_t i = 0; i < 29; i++)
		send_multiline_item(user, "item567890123456");
	send_multiline_fini(user, NULL);

	s = get_client_sendq(user);
	is_int(512, strlen(s), MSG);
	is_string("item567890123456\r\n", &s[494], MSG);
	is_client_sendq_empty(user, MSG);

	/* just run exactly the same thing again, there's static state */
	send_multiline_init(user, " ", "prefix78901234567 ");
	for (size_t i = 0; i < 29; i++)
		send_multiline_item(user, "item567890123456");
	send_multiline_fini(user, NULL);

	s = get_client_sendq(user);
	is_int(512, strlen(s), MSG);
	is_string("item567890123456\r\n", &s[494], MSG);
	is_client_sendq_empty(user, MSG);

	/* the same thing again but with one extra character, so we have an item that just won't fit */
	send_multiline_init(user, " ", "prefix789012345678 ");
	for (size_t i = 0; i < 29; i++)
		send_multiline_item(user, "item567890123456");
	send_multiline_item(user, "bar");
	send_multiline_fini(user, "foo ");

	s = get_client_sendq(user);
	is_string("item567890123456\r\n", &s[478], MSG);
	is_client_sendq("foo item567890123456 bar\r\n", user, MSG);

	remove_local_person(user);
}

static void sendto_multiline_extra_space(void)
{
	struct Client *server = make_remote_server_full(&me, "remote.test", "777");
	struct Client *luser = make_local_person();
	struct Client *ruser = make_remote_person(server);
	const char *s;

	strcpy(ruser->id, "777000001");
	add_to_id_hash(ruser->id, ruser);

	/* ":me.test foo4567890123 local_test :" -> 22 + 13 = 35 */
	send_multiline_init(luser, " ", ":%s foo4567890123 %s :",
			get_id(&me, luser), get_id(luser, luser));
	/* both of these should be noop */
	send_multiline_remote_pad(luser, &me);
	send_multiline_remote_pad(luser, luser);
	/* so all this should fit on one line */
	for (size_t i = 0; i < 28; i++)
		send_multiline_item(luser, "item567890123456");
	send_multiline_fini(luser, NULL);

	s = get_client_sendq(luser);
	is_int(512, strlen(s), MSG);
	is_string("item567890123456\r\n", &s[494], MSG);
	is_client_sendq_empty(luser, MSG);

	/* as above, but remote_test is one longer */
	send_multiline_init(ruser, " ", ":%s foo456789012 %s :",
			get_id(&me, ruser), get_id(ruser, ruser));
	/* should add "me.test" - 3 = 4 */
	send_multiline_remote_pad(ruser, &me);
	/* should add "remote_test" - 9 = 2 */
	send_multiline_remote_pad(ruser, ruser);
	/* so all this should fit on one line */
	for (size_t i = 0; i < 28; i++)
		send_multiline_item(ruser, "item567890123456");
	/* and this shouldn't */
	send_multiline_item(ruser, "x");
	send_multiline_fini(ruser, NULL);

	s = get_client_sendq(server);
	is_int(506, strlen(s), MSG);
	is_string("item567890123456\r\n", &s[488], MSG);
	is_client_sendq(":0AA foo456789012 777000001 :x\r\n", server, MSG);
}

int main(int argc, char *argv[])
{
	plan_lazy();

	ircd_util_init(__FILE__);
	client_util_init();

	sendto_multiline_basic();
	sendto_multiline_extra_space();

	client_util_free();
	ircd_util_free();
	return 0;
}
