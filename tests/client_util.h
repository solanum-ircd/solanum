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

#include "stdinc.h"
#include "ircd_defs.h"
#include "msg.h"
#include "client.h"

#include "ircd_util.h"

#define TEST_NICK "local_test"
#define TEST_USERNAME "username"
#define TEST_HOSTNAME "example.test"
#define TEST_IP "2001:db8::1:5ee:bad:c0de"
#define TEST_REALNAME "Test user"
#define TEST_ID TEST_ME_ID "00000"

#define TEST_ID_SUFFIX "!" TEST_USERNAME "@" TEST_HOSTNAME

#define TEST_SERVER_NAME "remote.test"
#define TEST_SERVER_ID "1BB"

#define TEST_REMOTE_NICK "remote_test"
#define TEST_REMOTE_ID TEST_SERVER_ID "00001"

#define TEST_SERVER2_NAME "remote2.test"
#define TEST_SERVER2_ID "2CC"

#define TEST_REMOTE2_NICK "remote2_test"
#define TEST_REMOTE2_ID TEST_SERVER2_ID "00002"

#define TEST_SERVER3_NAME "remote3.test"
#define TEST_SERVER3_ID "3DD"

#define TEST_REMOTE3_NICK "remote3_test"
#define TEST_REMOTE3_ID TEST_SERVER3_ID "00003"

#define TEST_CHANNEL "#test"

#define CRLF "\r\n"

void client_util_init(void);
void client_util_free(void);

struct Client *make_local_unknown(void);
struct Client *make_local_person(void);
struct Client *make_local_person_nick(const char *nick);
struct Client *make_local_person_id(const char *nick, const char *id);
struct Client *make_local_person_full(const char *nick, const char *username, const char *hostname, const char *ip, const char *realname);
struct Client *make_local_person_full_id(const char *nick, const char *username, const char *hostname, const char *ip, const char *realname, const char *id);
void make_local_person_oper(struct Client *client);
void remove_local_person(struct Client *client);

struct Client *make_remote_server(struct Client *uplink);
struct Client *make_remote_server_name(struct Client *uplink, const char *name);
struct Client *make_remote_server_full(struct Client *uplink, const char *name, const char *id);
struct Client *make_remote_person(struct Client *server);
struct Client *make_remote_person_nick(struct Client *server, const char *nick);
struct Client *make_remote_person_id(struct Client *server, const char *nick, const char *id);
struct Client *make_remote_person_full(struct Client *server, const char *nick, const char *username, const char *hostname, const char *ip, const char *realname);
struct Client *make_remote_person_full_id(struct Client *server, const char *nick, const char *username, const char *hostname, const char *ip, const char *realname, const char *id);
void make_remote_person_oper(struct Client *client);
void remove_remote_person(struct Client *client);
void remove_remote_server(struct Client *server);

struct Channel *make_channel(void);

char *get_client_sendq(const struct Client *client);
void drain_client_sendq(const struct Client *client);

void client_util_parse(struct Client *client, const char *message);

#define is_client_sendq_empty(client, message, ...) do { \
		is_string("", get_client_sendq(client), message, ##__VA_ARGS__); \
	} while (0)

#define is_client_sendq_one(queue, client, message, ...) do { \
		is_string(queue, get_client_sendq(client), message, ##__VA_ARGS__); \
	} while (0)

#define is_client_sendq(queue, client, message, ...) do { \
		is_client_sendq_one(queue, client, message, ##__VA_ARGS__); \
		is_client_sendq_empty(client, message, ##__VA_ARGS__); \
	} while (0)
