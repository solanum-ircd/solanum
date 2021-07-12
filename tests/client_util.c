/*
 *  client_util.c: Utility functions for making test clients
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "tap/basic.h"

#include "client_util.h"

#include "hash.h"
#include "s_newconf.h"
#include "parse.h"
#include "listener.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__

static struct Listener fake_listener = {
	.name = "fake",
	.F = NULL,
	.ref_count = 0,
	.active = 1,
	.ssl = 1,
	.defer_accept = 0,
	.sctp = false,
	.wsock = 0,
	.addr = {
		{ .ss_family = AF_INET6 },
		{ .ss_family = AF_INET6 },
	},
	.vhost = { "fake" },
};

struct Client *make_local_unknown(void)
{
	struct Client *client;

	client = make_client(NULL);
	rb_dlinkMoveNode(&client->localClient->tnode, &unknown_list, &lclient_list);
	client->servptr = &me;
	rb_dlinkAdd(client, &client->lnode, &client->servptr->serv->users);
	client->localClient->listener = &fake_listener;
	client->preClient->auth.accepted = true;
	client->localClient->localflags |= LFLAGS_FAKE;

	return client;
}

struct Client *make_local_person(void)
{
	return make_local_person_nick(TEST_NICK);
}

struct Client *make_local_person_nick(const char *nick)
{
	return make_local_person_full(nick, TEST_USERNAME, TEST_HOSTNAME, TEST_IP, TEST_REALNAME);
}

struct Client *make_local_person_full(const char *nick, const char *username, const char *hostname, const char *ip, const char *realname)
{
	struct Client *client;

	client = make_local_unknown();
	make_user(client);
	SetClient(client);

	rb_inet_pton_sock(ip, &client->localClient->ip);
	rb_strlcpy(client->name, nick, sizeof(client->name));
	rb_strlcpy(client->username, username, sizeof(client->username));
	rb_strlcpy(client->host, hostname, sizeof(client->host));
	rb_inet_ntop_sock((struct sockaddr *)&client->localClient->ip, client->sockhost, sizeof(client->sockhost));
	rb_strlcpy(client->info, realname, sizeof(client->info));

	add_to_client_hash(client->name, client);

	return client;
}

void make_local_person_oper(struct Client *client)
{
	rb_dlinkAddAlloc(client, &local_oper_list);
	rb_dlinkAddAlloc(client, &oper_list);
	SetOper(client);
	struct PrivilegeSet *p = privilegeset_set_new("test", "test:test", 0);
	client->user->privset = privilegeset_ref(p);
}

void remove_local_person(struct Client *client)
{
	exit_client(NULL, client, &me, "Test client removed");
}

struct Client *make_remote_server(struct Client *uplink)
{
	return make_remote_server_name(uplink, TEST_SERVER_NAME);
}

struct Client *make_remote_server_name(struct Client *uplink, const char *name)
{
	return make_remote_server_full(uplink, name, "");
}

struct Client *make_remote_server_full(struct Client *uplink, const char *name, const char *id)
{
	struct Client *client;

	client = make_client(NULL);
	client->servptr = uplink;
	client->localClient->localflags |= LFLAGS_FAKE;

	attach_server_conf(client, find_server_conf(name));

	rb_strlcpy(client->name, name, sizeof(client->name));
	rb_strlcpy(client->id, id, sizeof(client->id));

	rb_dlinkAdd(client, &client->lnode, &uplink->serv->servers);
	rb_dlinkMoveNode(&client->localClient->tnode, &unknown_list, &serv_list);
	rb_dlinkAddTailAlloc(client, &global_serv_list);

	make_server(client);
	SetServer(client);

	add_to_client_hash(client->name, client);
	if (strlen(id))
		add_to_id_hash(client->id, client);

	return client;
}

struct Client *make_remote_person(struct Client *server)
{
	return make_remote_person_nick(server, TEST_REMOTE_NICK);
}

struct Client *make_remote_person_nick(struct Client *server, const char *nick)
{
	return make_remote_person_full(server, nick, TEST_USERNAME, TEST_HOSTNAME, TEST_IP, TEST_REALNAME);
}

struct Client *make_remote_person_full(struct Client *server, const char *nick, const char *username, const char *hostname, const char *ip, const char *realname)
{
	struct Client *client;
	struct sockaddr_storage addr;

	client = make_client(server);
	make_user(client);
	SetRemoteClient(client);

	client->servptr = server;
	rb_dlinkAdd(server, &server->lnode, &server->servptr->serv->users);

	rb_inet_pton_sock(ip, &addr);
	rb_strlcpy(client->name, nick, sizeof(client->name));
	rb_strlcpy(client->username, username, sizeof(client->username));
	rb_strlcpy(client->host, hostname, sizeof(client->host));
	rb_inet_ntop_sock((struct sockaddr *)&addr, client->sockhost, sizeof(client->sockhost));
	rb_strlcpy(client->info, realname, sizeof(client->info));

	add_to_client_hash(nick, client);
	add_to_hostname_hash(client->host, client);

	return client;
}

void make_remote_person_oper(struct Client *client)
{
	rb_dlinkAddAlloc(client, &oper_list);
	SetOper(client);
}

void remove_remote_person(struct Client *client)
{
	exit_client(client, client->servptr, client->servptr, "Test client removed");
}

void remove_remote_server(struct Client *server)
{
	exit_client(server, server, server->servptr, "Test server removed");
}

struct Channel *make_channel(void)
{
	return allocate_channel(TEST_CHANNEL);
}

char *get_client_sendq(const struct Client *client)
{
	static char buf[EXT_BUFSIZE + sizeof(CRLF)];

	if (rb_linebuf_len(&client->localClient->buf_sendq)) {
		int ret;

		memset(buf, 0, sizeof(buf));
		ret = rb_linebuf_get(&client->localClient->buf_sendq, buf, sizeof(buf), 0, 1);

		if (ok(ret > 0, MSG)) {
			return buf;
		} else {
			return "<get_client_sendq error>";
		}
	}

	return "";
}

void client_util_parse(struct Client *client, const char *message)
{
	char *copy = rb_strdup(message);

	parse(client, copy, copy+strlen(copy));

	rb_free(copy);
}

void client_util_init(void)
{
}

void client_util_free(void)
{
}
