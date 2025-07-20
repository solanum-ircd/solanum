/*
 *  serv_connect_exit_unknown_client1.c: Test serv_connect followed by exit_unknown_client
 *  Copyright 2017 Simon Arlott
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

#include "rb_lib.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__

static rb_fde_t *last_F = NULL;
static CNCB *last_connect_callback = NULL;
static void *last_connect_data = NULL;

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	printf("# connect(%d, ...)\n", sockfd);
	errno = EINPROGRESS;
	return -1;
}

void rb_connect_tcp(rb_fde_t *F, struct sockaddr *dest, struct sockaddr *clocal, CNCB *callback, void *data, int timeout)
{
	printf("# rb_connect_tcp(%p, ...)\n", F);

	last_F = F;
	last_connect_callback = callback;
	last_connect_data = data;

	void *(*func)(rb_fde_t *, struct sockaddr *, struct sockaddr *, CNCB *, void *, int) = dlsym(RTLD_NEXT, "rb_connect_tcp");
	func(F, dest, clocal, callback, data, timeout);
}

static void basic_test(void)
{
	struct server_conf *server = NULL;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, server_conf_list.head)
        {
		server = ptr->data;
	}

	/* The server doesn't exist before */
	ok(find_server(NULL, server->name) == NULL, MSG);

	/* The server doesn't exist during the connection attempt */
	is_int(1, serv_connect(server, NULL), MSG);
	ok(find_server(NULL, server->name) == NULL, MSG);

	if (ok(last_connect_callback != NULL, MSG)) {
		last_connect_callback(last_F, RB_ERR_CONNECT, last_connect_data);
	}

	/* The server doesn't exist after the connection attempt fails */
	ok(find_server(NULL, server->name) == NULL, MSG);
}

static void incoming_during_outgoing(void)
{
	struct server_conf *server = NULL;
	struct Client *incoming = NULL;
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, server_conf_list.head)
        {
		server = ptr->data;
	}

	printf("# Test server = %s\n", server->name);

	/* The server doesn't exist before */
	ok(find_server(NULL, server->name) == NULL, MSG);

	/* The server doesn't exist during the connection attempt */
	is_int(1, serv_connect(server, NULL), MSG);
	ok(find_server(NULL, server->name) == NULL, MSG);

	/* The server makes its own incoming connection */
	incoming = make_remote_server_full(&me, server->name, TEST_SERVER_ID);
	ok(find_server(NULL, server->name) == incoming, MSG);
	ok(find_id(TEST_SERVER_ID) == incoming, MSG);

	if (ok(last_connect_callback != NULL, MSG)) {
		/* This will call exit_unknown_client on our outgoing connection */
		last_connect_callback(last_F, RB_ERR_CONNECT, last_connect_data);
	}

	/* The incoming server should still be here */
	ok(find_server(NULL, server->name) == incoming, MSG);
	ok(find_id(TEST_SERVER_ID) == incoming, MSG);

	remove_remote_server(incoming);

	ok(find_server(NULL, server->name) == NULL, MSG);
	ok(find_id(TEST_SERVER_ID) == NULL, MSG);
}

int main(int argc, char *argv[])
{
	plan_lazy();

	ircd_util_init(__FILE__);
	client_util_init();

	basic_test();
	incoming_during_outgoing();

	client_util_free();
	ircd_util_free();
	return 0;
}
