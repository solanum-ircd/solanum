/*
 *  sasl_abort1.c: Test SASL abort from the ircd to services
 *  Copyright 2019 Simon Arlott
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
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "tap/basic.h"

#include "ircd_util.h"
#include "client_util.h"

#include "s_serv.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "hash.h"

#define MSG "%s:%d (%s; aborted=%d)", __FILE__, __LINE__, __FUNCTION__, aborted

static void common_sasl_test(bool aborted)
{
	ircd_util_init(__FILE__);
	client_util_init();

	struct Client *user = make_local_unknown();
	struct Client *server = make_remote_server(&me);
	struct Client *remote = make_remote_person(server);

	rb_inet_pton_sock(TEST_IP, &user->localClient->ip);
	rb_strlcpy(user->host, TEST_HOSTNAME, sizeof(user->host));
	rb_inet_ntop_sock((struct sockaddr *)&user->localClient->ip, user->sockhost, sizeof(user->sockhost));

	strcpy(server->id, TEST_SERVER_ID);
	strcpy(remote->id, TEST_REMOTE_ID);
	add_to_id_hash(remote->id, remote);
	server->localClient->caps = CAP_ENCAP | CAP_TS6;
	remote->umodes |= UMODE_SERVICE;

	client_util_parse(user, "CAP LS 302" CRLF);
	const char *line;
	while ((line = get_client_sendq(user)) && strcmp(line, "")) {
		printf("%s", line);
	}

	client_util_parse(user, "NICK " TEST_NICK CRLF);
	client_util_parse(user, "USER " TEST_USERNAME " 0 0 :" TEST_REALNAME CRLF);
	is_client_sendq_empty(user, MSG);

	user->tsinfo = 42;

	client_util_parse(user, "CAP REQ :sasl" CRLF);
	is_client_sendq(":" TEST_ME_NAME " CAP " TEST_NICK " ACK :sasl" CRLF, user, MSG);

	client_util_parse(user, "AUTHENTICATE EXTERNAL" CRLF);
	is_client_sendq_empty(user, MSG);

	is_client_sendq_one(":" TEST_ME_ID " ENCAP " TEST_SERVER_NAME " SASL " TEST_ME_ID "AAAAAB " TEST_REMOTE_ID " H " TEST_HOSTNAME " " TEST_IP " P" CRLF, server, MSG);
	is_client_sendq_one(":" TEST_ME_ID " ENCAP " TEST_SERVER_NAME " SASL " TEST_ME_ID "AAAAAB " TEST_REMOTE_ID " S EXTERNAL" CRLF, server, MSG);
	is_client_sendq_empty(server, MSG);

	if (aborted) {
		// Explicit abort by user
		client_util_parse(user, "AUTHENTICATE *" CRLF);
		is_client_sendq(":" TEST_ME_NAME " 906 " TEST_NICK " :SASL authentication aborted" CRLF, user, MSG);

		client_util_parse(user, "CAP END" CRLF);
		ok(IsClient(user), MSG);

		is_client_sendq_one(":" TEST_ME_ID " ENCAP " TEST_SERVER_NAME " SASL " TEST_ME_ID "AAAAAB " TEST_REMOTE_ID " D A" CRLF, server, MSG);
		is_client_sendq(":" TEST_ME_ID " UID " TEST_NICK " 1 42 +i ~" TEST_USERNAME " " TEST_HOSTNAME " " TEST_IP " " TEST_ME_ID "AAAAAB :" TEST_REALNAME CRLF, server, MSG);
	} else {
		// Return a successful auth
		client_util_parse(server, ":" TEST_SERVER_NAME " ENCAP " TEST_ME_NAME " SASL " TEST_REMOTE_ID " " TEST_ME_ID "AAAAAB D S" CRLF);

		// User should be authenticated
		is_client_sendq_one(":" TEST_ME_NAME " 903 " TEST_NICK " :SASL authentication successful" CRLF, user, MSG);

		client_util_parse(user, "CAP END" CRLF);
		ok(IsClient(user), MSG);
	}

	is_client_sendq_one(":" TEST_ME_NAME " 001 " TEST_NICK " :Welcome to the Test Internet Relay Chat Network " TEST_NICK CRLF, user, MSG);
	while ((line = get_client_sendq(user)) && strcmp(line, "")) {
		printf("%s", line);
	}

	if (aborted) {
		// Return a successful auth after auth was aborted
		client_util_parse(server, ":" TEST_SERVER_NAME " ENCAP " TEST_ME_NAME " SASL " TEST_REMOTE_ID " " TEST_ME_ID "AAAAAB D S" CRLF);

		// User should not be authenticated
		is_client_sendq_empty(user, MSG);
	}

	remove_local_person(user);
	remove_remote_person(remote);
	remove_remote_server(server);

	client_util_free();
	ircd_util_free();
}

static void successful_login(void)
{
	common_sasl_test(false);
}

static void successful_login_after_aborted_by_user(void)
{
	common_sasl_test(true);
}

int main(int argc, char *argv[])
{
	plan_lazy();

	successful_login();
	successful_login_after_aborted_by_user();

	return 0;
}
