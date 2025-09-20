/*
 *  send1.c: Test sendto_* under various conditions
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

#include "ircd_util.h"
#include "client_util.h"

#include "send.h"
#include "s_serv.h"
#include "monitor.h"
#include "s_conf.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__

// What time is it?
#define ADVENTURE_TIME "2017-07-14T02:40:00.000Z"

int rb_gettimeofday(struct timeval *tv, void *tz)
{
	if (tv == NULL) {
		errno = EFAULT;
		return -1;
	}
	tv->tv_sec = 1500000000;
	tv->tv_usec = 0;
	return 0;
}

unsigned int CAP_ACCOUNT_TAG;
unsigned int CAP_SERVER_TIME;
unsigned int CAP_MULTI_PREFIX;

static struct Client *user;
static struct Client *server;
static struct Client *remote;
static struct Client *server2;
static struct Client *remote2;
static struct Client *server3;
static struct Client *remote3;
static struct Channel *channel;
static struct Channel *lchannel;

static struct Client *local_chan_o;
static struct Client *local_chan_ov;
static struct Client *local_chan_v;
static struct Client *local_chan_p;
static struct Client *local_chan_d;
static struct Client *local_no_chan;

static struct Client *remote_chan_o;
static struct Client *remote_chan_ov;
static struct Client *remote_chan_v;
static struct Client *remote_chan_p;
static struct Client *remote_chan_d;

static struct Client *remote2_chan_p;
static struct Client *remote2_chan_d;

static void standard_init(void)
{
	user = make_local_person();
	server = make_remote_server(&me);
	remote = make_remote_person(server);
	server2 = make_remote_server_name(&me, TEST_SERVER2_NAME);
	remote2 = make_remote_person_nick(server2, TEST_REMOTE2_NICK);
	server3 = make_remote_server_name(&me, TEST_SERVER3_NAME);
	remote3 = make_remote_person_nick(server3, TEST_REMOTE3_NICK);

	// Expose potential bugs in overlapping capabilities
	server->localClient->caps |= CAP_ACCOUNT_TAG;
	server->localClient->caps |= CAP_SERVER_TIME;
	server2->localClient->caps |= CAP_ACCOUNT_TAG;
	server2->localClient->caps |= CAP_SERVER_TIME;
	server3->localClient->caps |= CAP_ACCOUNT_TAG;
	server3->localClient->caps |= CAP_SERVER_TIME;

	local_chan_o = make_local_person_nick("LChanOp");
	local_chan_ov = make_local_person_nick("LChanOpVoice");
	local_chan_v = make_local_person_nick("LChanVoice");
	local_chan_p = make_local_person_nick("LChanPeon");
	local_chan_d = make_local_person_nick("LChanDeaf");
	local_chan_d->umodes |= UMODE_DEAF;
	local_no_chan = make_local_person_nick("LNoChan");

	remote_chan_o = make_remote_person_nick(server, "RChanOp");
	remote_chan_ov = make_remote_person_nick(server, "RChanOpVoice");
	remote_chan_v = make_remote_person_nick(server, "RChanVoice");
	remote_chan_p = make_remote_person_nick(server, "RChanPeon");
	remote_chan_d = make_remote_person_nick(server, "RChanDeaf");
	remote_chan_d->umodes |= UMODE_DEAF;

	remote2_chan_p = make_remote_person_nick(server2, "R2ChanPeon");
	remote2_chan_d = make_remote_person_nick(server2, "R2ChanDeaf");
	remote2_chan_d->umodes |= UMODE_DEAF;

	channel = make_channel();

	add_user_to_channel(channel, local_chan_o, CHFL_CHANOP);
	add_user_to_channel(channel, local_chan_ov, CHFL_CHANOP | CHFL_VOICE);
	add_user_to_channel(channel, local_chan_v, CHFL_VOICE);
	add_user_to_channel(channel, local_chan_p, CHFL_PEON);
	add_user_to_channel(channel, local_chan_d, CHFL_CHANOP | CHFL_VOICE);

	add_user_to_channel(channel, remote_chan_o, CHFL_CHANOP);
	add_user_to_channel(channel, remote_chan_ov, CHFL_CHANOP | CHFL_VOICE);
	add_user_to_channel(channel, remote_chan_v, CHFL_VOICE);
	add_user_to_channel(channel, remote_chan_p, CHFL_PEON);
	add_user_to_channel(channel, remote_chan_d, CHFL_CHANOP | CHFL_VOICE);

	add_user_to_channel(channel, remote2_chan_p, CHFL_PEON);
	add_user_to_channel(channel, remote2_chan_d, CHFL_CHANOP | CHFL_VOICE);

	lchannel = allocate_channel("&test");

	add_user_to_channel(lchannel, user, CHFL_PEON);
	add_user_to_channel(lchannel, remote, CHFL_PEON);
	add_user_to_channel(lchannel, remote2, CHFL_PEON);
	add_user_to_channel(lchannel, remote3, CHFL_PEON);
}

static void standard_ids(void)
{
	strcpy(user->id, TEST_ID);
	strcpy(server->id, TEST_SERVER_ID);
	strcpy(remote->id, TEST_REMOTE_ID);
	strcpy(server2->id, TEST_SERVER2_ID);
	strcpy(remote2->id, TEST_REMOTE2_ID);
	strcpy(server3->id, TEST_SERVER3_ID);
	strcpy(remote3->id, TEST_REMOTE3_ID);

	strcpy(local_chan_o->id, TEST_ME_ID "90001");
	strcpy(local_chan_ov->id, TEST_ME_ID "90002");
	strcpy(local_chan_v->id, TEST_ME_ID "90003");
	strcpy(local_chan_p->id, TEST_ME_ID "90004");
	strcpy(local_chan_d->id, TEST_ME_ID "90005");

	strcpy(remote_chan_o->id, TEST_SERVER_ID "90101");
	strcpy(remote_chan_ov->id, TEST_SERVER_ID "90102");
	strcpy(remote_chan_v->id, TEST_SERVER_ID "90103");
	strcpy(remote_chan_p->id, TEST_SERVER_ID "90104");
	strcpy(remote_chan_d->id, TEST_SERVER_ID "90105");

	strcpy(remote2_chan_p->id, TEST_SERVER2_ID "90204");
	strcpy(remote2_chan_d->id, TEST_SERVER2_ID "90205");
}

static void standard_server_caps(unsigned int add, unsigned int remove)
{
	server->localClient->caps |= add;
	server2->localClient->caps |= add;
	server3->localClient->caps |= add;

	server->localClient->caps &= ~remove;
	server2->localClient->caps &= ~remove;
	server3->localClient->caps &= ~remove;
}

static void standard_free(void)
{
	remove_remote_person(remote2_chan_p);
	remove_remote_person(remote2_chan_d);

	remove_remote_person(remote_chan_o);
	remove_remote_person(remote_chan_ov);
	remove_remote_person(remote_chan_v);
	remove_remote_person(remote_chan_p);
	remove_remote_person(remote_chan_d);

	remove_local_person(local_chan_o);
	remove_local_person(local_chan_ov);
	remove_local_person(local_chan_v);
	remove_local_person(local_chan_p);
	remove_local_person(local_chan_d);
	remove_local_person(local_no_chan);

	remove_remote_person(remote3);
	remove_remote_server(server3);
	remove_remote_person(remote2);
	remove_remote_server(server2);
	remove_remote_person(remote);
	remove_remote_server(server);
	remove_local_person(user);
}

static void sendto_one1(void)
{
	standard_init();

	sendto_one(user, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, user, MSG);

	sendto_one(server, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);

	sendto_one(remote, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);

	standard_free();
}

static void sendto_one1__tags(void)
{
	standard_init();

	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_one(local_chan_o, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_o, MSG);

	sendto_one(local_chan_ov, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, MSG);

	sendto_one(local_chan_v, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, local_chan_v, MSG);

	sendto_one(local_chan_p, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, local_chan_p, MSG);

	standard_free();
}

static void sendto_one_prefix1(void)
{
	standard_init();

	// Local
	sendto_one_prefix(user, &me, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " TEST " TEST_NICK " :Hello World!" CRLF, user, MSG);

	sendto_one_prefix(user, user, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_NICK " TEST " TEST_NICK " :Hello World!" CRLF, user, MSG);

	sendto_one_prefix(user, remote, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE_NICK " TEST " TEST_NICK " :Hello World!" CRLF, user, MSG);

	sendto_one_prefix(user, server, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER_NAME " TEST " TEST_NICK " :Hello World!" CRLF, user, MSG);

	// Remote (without ID)
	sendto_one_prefix(remote, &me, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " TEST " TEST_REMOTE_NICK " :Hello World!" CRLF, server, MSG);

	sendto_one_prefix(remote, user, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_NICK " TEST " TEST_REMOTE_NICK " :Hello World!" CRLF, server, MSG);

	sendto_one_prefix(remote, remote, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE_NICK " TEST " TEST_REMOTE_NICK " :Hello World!" CRLF, server, MSG);

	sendto_one_prefix(remote, server, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER_NAME " TEST " TEST_REMOTE_NICK " :Hello World!" CRLF, server, MSG);

	standard_ids();

	// Remote (with ID)
	sendto_one_prefix(remote, &me, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " TEST " TEST_REMOTE_ID " :Hello World!" CRLF, server, MSG);

	sendto_one_prefix(remote, user, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_ID " TEST " TEST_REMOTE_ID " :Hello World!" CRLF, server, MSG);

	sendto_one_prefix(remote, remote, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE_ID " TEST " TEST_REMOTE_ID " :Hello World!" CRLF, server, MSG);

	sendto_one_prefix(remote, server, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER_ID " TEST " TEST_REMOTE_ID " :Hello World!" CRLF, server, MSG);

	standard_free();
}

static void sendto_one_prefix1__tags(void)
{
	standard_init();

	strcpy(user->user->suser, "test");
	strcpy(remote->user->suser, "rtest");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_one_prefix(user, &me, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " TEST " TEST_NICK " :Hello World!" CRLF, user, MSG);

	sendto_one_prefix(user, user, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_NICK " TEST " TEST_NICK " :Hello World!" CRLF, user, MSG);

	sendto_one_prefix(user, remote, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE_NICK " TEST " TEST_NICK " :Hello World!" CRLF, user, MSG);

	sendto_one_prefix(user, server, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER_NAME " TEST " TEST_NICK " :Hello World!" CRLF, user, MSG);


	sendto_one_prefix(local_chan_o, &me, "TEST", ":Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " TEST LChanOp :Hello World!" CRLF, local_chan_o, MSG);

	sendto_one_prefix(local_chan_o, user, "TEST", ":Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test :" TEST_NICK " TEST LChanOp :Hello World!" CRLF, local_chan_o, MSG);

	sendto_one_prefix(local_chan_o, remote, "TEST", ":Hello %s!", "World");
	is_client_sendq("@account=rtest :" TEST_REMOTE_NICK " TEST LChanOp :Hello World!" CRLF, local_chan_o, MSG);

	sendto_one_prefix(local_chan_o, server, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER_NAME " TEST LChanOp :Hello World!" CRLF, local_chan_o, MSG);


	sendto_one_prefix(local_chan_ov, &me, "TEST", ":Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " TEST LChanOpVoice :Hello World!" CRLF, local_chan_ov, MSG);

	sendto_one_prefix(local_chan_ov, user, "TEST", ":Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_NICK " TEST LChanOpVoice :Hello World!" CRLF, local_chan_ov, MSG);

	sendto_one_prefix(local_chan_ov, remote, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE_NICK " TEST LChanOpVoice :Hello World!" CRLF, local_chan_ov, MSG);

	sendto_one_prefix(local_chan_ov, server, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER_NAME " TEST LChanOpVoice :Hello World!" CRLF, local_chan_ov, MSG);


	sendto_one_prefix(local_chan_v, &me, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " TEST LChanVoice :Hello World!" CRLF, local_chan_v, MSG);

	sendto_one_prefix(local_chan_v, user, "TEST", ":Hello %s!", "World");
	is_client_sendq("@account=test :" TEST_NICK " TEST LChanVoice :Hello World!" CRLF, local_chan_v, MSG);

	sendto_one_prefix(local_chan_v, remote, "TEST", ":Hello %s!", "World");
	is_client_sendq("@account=rtest :" TEST_REMOTE_NICK " TEST LChanVoice :Hello World!" CRLF, local_chan_v, MSG);

	sendto_one_prefix(local_chan_v, server, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER_NAME " TEST LChanVoice :Hello World!" CRLF, local_chan_v, MSG);


	sendto_one_prefix(local_chan_p, &me, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " TEST LChanPeon :Hello World!" CRLF, local_chan_p, MSG);

	sendto_one_prefix(local_chan_p, user, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_NICK " TEST LChanPeon :Hello World!" CRLF, local_chan_p, MSG);

	sendto_one_prefix(local_chan_p, remote, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE_NICK " TEST LChanPeon :Hello World!" CRLF, local_chan_p, MSG);

	sendto_one_prefix(local_chan_p, server, "TEST", ":Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER_NAME " TEST LChanPeon :Hello World!" CRLF, local_chan_p, MSG);

	standard_free();
}

static void sendto_one_notice1(void)
{
	standard_init();

	// Local
	sendto_one_notice(user, ":Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " NOTICE " TEST_NICK " :Hello World!" CRLF, user, MSG);

	// Remote (without ID)
	sendto_one_notice(remote, ":Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " NOTICE " TEST_REMOTE_NICK " :Hello World!" CRLF, server, MSG);

	standard_ids();

	// Remote (with ID)
	sendto_one_notice(remote, ":Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " NOTICE " TEST_REMOTE_ID " :Hello World!" CRLF, server, MSG);

	// Local (unregistered)
	user->name[0] = '\0';
	sendto_one_notice(user, ":Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :Hello World!" CRLF, user, MSG);

	standard_free();
}

static void sendto_one_notice1__tags(void)
{
	standard_init();

	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_one_notice(local_chan_o, ":Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE LChanOp :Hello World!" CRLF, local_chan_o, MSG);

	sendto_one_notice(local_chan_ov, ":Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE LChanOpVoice :Hello World!" CRLF, local_chan_ov, MSG);

	sendto_one_notice(local_chan_v, ":Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " NOTICE LChanVoice :Hello World!" CRLF, local_chan_v, MSG);

	// Unregistered
	local_chan_o->name[0] = '\0';
	local_chan_ov->name[0] = '\0';
	local_chan_v->name[0] = '\0';

	sendto_one_notice(local_chan_o, ":Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :Hello World!" CRLF, local_chan_o, MSG);

	sendto_one_notice(local_chan_ov, ":Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :Hello World!" CRLF, local_chan_ov, MSG);

	sendto_one_notice(local_chan_v, ":Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :Hello World!" CRLF, local_chan_v, MSG);

	standard_free();
}

static void sendto_one_numeric1(void)
{
	standard_init();

	// Local
	sendto_one_numeric(user, 1, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " 001 " TEST_NICK " Hello World!" CRLF, user, MSG);

	// Remote (without ID)
	sendto_one_numeric(server, 1, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " 001 " TEST_SERVER_NAME " Hello World!" CRLF, server, MSG);

	sendto_one_numeric(remote, 1, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " 001 " TEST_REMOTE_NICK " Hello World!" CRLF, server, MSG);

	standard_ids();

	// Remote (with ID)
	sendto_one_numeric(server, 1, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " 001 " TEST_SERVER_ID " Hello World!" CRLF, server, MSG);

	sendto_one_numeric(remote, 1, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " 001 " TEST_REMOTE_ID " Hello World!" CRLF, server, MSG);

	// Local (unregistered)
	user->name[0] = '\0';
	sendto_one_numeric(user, 1, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " 001 * Hello World!" CRLF, user, MSG);

	standard_free();
}

static void sendto_one_numeric1__tags(void)
{
	standard_init();

	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_one_numeric(local_chan_o, 1, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " 001 LChanOp Hello World!" CRLF, local_chan_o, MSG);

	sendto_one_numeric(local_chan_ov, 1, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " 001 LChanOpVoice Hello World!" CRLF, local_chan_ov, MSG);

	sendto_one_numeric(local_chan_v, 1, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " 001 LChanVoice Hello World!" CRLF, local_chan_v, MSG);

	// Unregistered
	local_chan_o->name[0] = '\0';
	local_chan_ov->name[0] = '\0';
	local_chan_v->name[0] = '\0';

	sendto_one_numeric(local_chan_o, 1, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " 001 * Hello World!" CRLF, local_chan_o, MSG);

	sendto_one_numeric(local_chan_ov, 1, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " 001 * Hello World!" CRLF, local_chan_ov, MSG);

	sendto_one_numeric(local_chan_v, 1, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " 001 * Hello World!" CRLF, local_chan_v, MSG);

	standard_free();
}

static void sendto_server1(void)
{
	standard_init();

	// TODO test capabilities

	// Local
	sendto_server(NULL, channel, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	sendto_server(NULL, lchannel, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_server(NULL, NULL, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	sendto_server(user, channel, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	sendto_server(user, lchannel, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_server(user, NULL, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	// Remote
	sendto_server(remote, channel, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	sendto_server(remote, lchannel, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_server(remote, NULL, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	sendto_server(remote2, channel, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	sendto_server(remote2, lchannel, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_server(remote2, NULL, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	sendto_server(remote3, channel, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_server(remote3, lchannel, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_server(remote3, NULL, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	standard_free();
}

static void sendto_server1__tags(void)
{
	standard_init();

	// TODO test capabilities

	strcpy(user->user->suser, "test");
	strcpy(remote->user->suser, "rtest");
	strcpy(remote2->user->suser, "r2test");
	strcpy(remote3->user->suser, "r3test");

	// Local
	sendto_server(NULL, channel, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	sendto_server(NULL, lchannel, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_server(NULL, NULL, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	sendto_server(user, channel, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	sendto_server(user, lchannel, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_server(user, NULL, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	// Remote
	sendto_server(remote, channel, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	sendto_server(remote, lchannel, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_server(remote, NULL, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	sendto_server(remote2, channel, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	sendto_server(remote2, lchannel, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_server(remote2, NULL, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq("Hello World!" CRLF, server3, MSG);

	sendto_server(remote3, channel, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_server(remote3, lchannel, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_server(remote3, NULL, 0, 0, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, server, MSG);
	is_client_sendq("Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	standard_free();
}

static void sendto_channel_flags__local__all_members(void)
{
	standard_init();

	sendto_channel_flags(local_chan_p, ALL_MEMBERS, local_chan_p, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":LChanPeon TEST #placeholder :Hello World!" CRLF, server, MSG);
	is_client_sendq(":LChanPeon TEST #placeholder :Hello World!" CRLF, server2, MSG);

	standard_ids();

	sendto_channel_flags(local_chan_p, ALL_MEMBERS, local_chan_p, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_ME_ID "90004 TEST #placeholder :Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ME_ID "90004 TEST #placeholder :Hello World!" CRLF, server2, MSG);

	standard_free();
}

static void sendto_channel_flags__remote__all_members(void)
{
	standard_init();

	sendto_channel_flags(server, ALL_MEMBERS, remote_chan_p, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq(":RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq(":RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq(":RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "Message source; " MSG);
	is_client_sendq(":RChanPeon TEST #placeholder :Hello World!" CRLF, server2, MSG);

	standard_ids();

	sendto_channel_flags(server, ALL_MEMBERS, remote_chan_p, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq(":RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq(":RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq(":RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "Message source; " MSG);
	is_client_sendq(":" TEST_SERVER_ID "90104 TEST #placeholder :Hello World!" CRLF, server2, MSG);

	standard_free();
}

static void sendto_channel_flags__local__all_members__tags(void)
{
	standard_init();

	strcpy(local_chan_p->user->suser, "test");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_channel_flags(local_chan_p, ALL_MEMBERS, local_chan_p, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test :LChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :LChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("@account=test :LChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":LChanPeon TEST #placeholder :Hello World!" CRLF, server, MSG);
	is_client_sendq(":LChanPeon TEST #placeholder :Hello World!" CRLF, server2, MSG);

	standard_ids();

	sendto_channel_flags(local_chan_p, ALL_MEMBERS, local_chan_p, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test :LChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :LChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("@account=test :LChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_ME_ID "90004 TEST #placeholder :Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ME_ID "90004 TEST #placeholder :Hello World!" CRLF, server2, MSG);

	standard_free();
}

static void sendto_channel_flags__remote__all_members__tags(void)
{
	standard_init();

	strcpy(remote_chan_p->user->suser, "test");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_channel_flags(server, ALL_MEMBERS, remote_chan_p, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq("@account=test :RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("@account=test :RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq(":RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "Message source; " MSG);
	is_client_sendq(":RChanPeon TEST #placeholder :Hello World!" CRLF, server2, MSG);

	standard_ids();

	sendto_channel_flags(server, ALL_MEMBERS, remote_chan_p, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq("@account=test :RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("@account=test :RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq(":RChanPeon" TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "Message source; " MSG);
	is_client_sendq(":" TEST_SERVER_ID "90104 TEST #placeholder :Hello World!" CRLF, server2, MSG);

	standard_free();
}

static void sendto_channel_flags__local__voice(void)
{
	standard_init();

	// Without CAP_CHW
	standard_server_caps(0, CAP_CHW);

	sendto_channel_flags(user, CHFL_VOICE, user, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "No users to receive message; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW
	standard_server_caps(CAP_CHW, 0);

	sendto_channel_flags(user, CHFL_VOICE, user, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_NICK " TEST #placeholder :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	standard_ids();

	// Without CAP_CHW
	standard_server_caps(0, CAP_CHW);

	sendto_channel_flags(user, CHFL_VOICE, user, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "No users to receive message; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW
	standard_server_caps(CAP_CHW, 0);

	sendto_channel_flags(user, CHFL_VOICE, user, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_ID " TEST #placeholder :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	standard_free();
}

static void sendto_channel_flags__remote__voice(void)
{
	standard_init();

	// Without CAP_CHW
	standard_server_caps(0, CAP_CHW);

	sendto_channel_flags(server3, CHFL_VOICE, remote3, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "No users to receive message; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW
	standard_server_caps(CAP_CHW, 0);

	sendto_channel_flags(server3, CHFL_VOICE, remote3, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK " TEST #placeholder :Hello World!" CRLF, server, "On channel; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	standard_ids();

	// Without CAP_CHW
	standard_server_caps(0, CAP_CHW);

	sendto_channel_flags(server3, CHFL_VOICE, remote3, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "No users to receive message; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW
	standard_server_caps(CAP_CHW, 0);

	sendto_channel_flags(server3, CHFL_VOICE, remote3, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_REMOTE3_ID " TEST #placeholder :Hello World!" CRLF, server, "On channel; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	standard_free();
}

static void sendto_channel_flags__local__chanop(void)
{
	standard_init();

	// Without CAP_CHW
	standard_server_caps(0, CAP_CHW);

	sendto_channel_flags(user, CHFL_CHANOP, user, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "No users to receive message; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW
	standard_server_caps(CAP_CHW, 0);

	sendto_channel_flags(user, CHFL_CHANOP, user, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_NICK " TEST #placeholder :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	standard_ids();

	// Without CAP_CHW
	standard_server_caps(0, CAP_CHW);

	sendto_channel_flags(user, CHFL_CHANOP, user, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "No users to receive message; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW
	standard_server_caps(CAP_CHW, 0);

	sendto_channel_flags(user, CHFL_CHANOP, user, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_ID " TEST #placeholder :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	standard_free();
}

static void sendto_channel_flags__remote__chanop(void)
{
	standard_init();

	// Without CAP_CHW
	standard_server_caps(0, CAP_CHW);

	sendto_channel_flags(server3, CHFL_CHANOP, remote3, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "No users to receive message; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW
	standard_server_caps(CAP_CHW, 0);

	sendto_channel_flags(server3, CHFL_CHANOP, remote3, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK " TEST #placeholder :Hello World!" CRLF, server, "On channel; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	standard_ids();

	// Without CAP_CHW
	standard_server_caps(0, CAP_CHW);

	sendto_channel_flags(server3, CHFL_CHANOP, remote3, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "No users to receive message; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW
	standard_server_caps(CAP_CHW, 0);

	sendto_channel_flags(server3, CHFL_CHANOP, remote3, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_REMOTE3_ID " TEST #placeholder :Hello World!" CRLF, server, "On channel; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	standard_free();
}

static void sendto_channel_flags__local__chanop_voice(void)
{
	standard_init();

	// Without CAP_CHW
	standard_server_caps(0, CAP_CHW);

	sendto_channel_flags(user, CHFL_CHANOP | CHFL_VOICE, user, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "No users to receive message; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW
	standard_server_caps(CAP_CHW, 0);

	sendto_channel_flags(user, CHFL_CHANOP | CHFL_VOICE, user, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_NICK " TEST #placeholder :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	standard_ids();

	// Without CAP_CHW
	standard_server_caps(0, CAP_CHW);

	sendto_channel_flags(user, CHFL_CHANOP | CHFL_VOICE, user, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "No users to receive message; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW
	standard_server_caps(CAP_CHW, 0);

	sendto_channel_flags(user, CHFL_CHANOP | CHFL_VOICE, user, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_ID " TEST #placeholder :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	standard_free();
}

static void sendto_channel_flags__remote__chanop_voice(void)
{
	standard_init();

	// Without CAP_CHW
	standard_server_caps(0, CAP_CHW);

	sendto_channel_flags(server3, CHFL_CHANOP | CHFL_VOICE, remote3, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "No users to receive message; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW
	standard_server_caps(CAP_CHW, 0);

	sendto_channel_flags(server3, CHFL_CHANOP | CHFL_VOICE, remote3, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK " TEST #placeholder :Hello World!" CRLF, server, "On channel; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	standard_ids();

	// Without CAP_CHW
	standard_server_caps(0, CAP_CHW);

	sendto_channel_flags(server3, CHFL_CHANOP | CHFL_VOICE, remote3, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "No users to receive message; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW
	standard_server_caps(CAP_CHW, 0);

	sendto_channel_flags(server3, CHFL_CHANOP | CHFL_VOICE, remote3, channel, "TEST #placeholder :Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq(":" TEST_REMOTE3_NICK TEST_ID_SUFFIX " TEST #placeholder :Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_REMOTE3_ID " TEST #placeholder :Hello World!" CRLF, server, "On channel; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	standard_free();
}

static void sendto_channel_opmod__local(void)
{
	standard_init();

	// This function does not support TS5...
	standard_ids();

	// Without CAP_CHW | CAP_EOPMOD
	standard_server_caps(0, CAP_CHW | CAP_EOPMOD);

	sendto_channel_opmod(local_chan_p, local_chan_p, channel, "TEST", "Hello World!");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "No users to receive message; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW, without CAP_EOPMOD
	standard_server_caps(CAP_CHW, CAP_EOPMOD);

	sendto_channel_opmod(local_chan_p, local_chan_p, channel, "TEST", "Hello World!");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_ME_ID " NOTICE @" TEST_CHANNEL " :<LChanPeon:" TEST_CHANNEL "> Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// Moderated channel
	channel->mode.mode |= MODE_MODERATED;

	sendto_channel_opmod(local_chan_p, local_chan_p, channel, "TEST", "Hello World!");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_ME_ID "90004 TEST " TEST_CHANNEL " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW | CAP_EOPMOD
	channel->mode.mode &= ~MODE_MODERATED;
	standard_server_caps(CAP_CHW | CAP_EOPMOD, 0);

	sendto_channel_opmod(local_chan_p, local_chan_p, channel, "TEST", "Hello World!");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_ME_ID "90004 TEST =" TEST_CHANNEL " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	standard_free();
}

static void sendto_channel_opmod_statusmsg__local(void)
{
	standard_init();
	ConfigChannel.opmod_send_statusmsg = true;

	// This function does not support TS5...
	standard_ids();

	// Without CAP_CHW | CAP_EOPMOD
	standard_server_caps(0, CAP_CHW | CAP_EOPMOD);

	sendto_channel_opmod(local_chan_p, local_chan_p, channel, "TEST", "Hello World!");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "No users to receive message; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW, without CAP_EOPMOD
	standard_server_caps(CAP_CHW, CAP_EOPMOD);

	sendto_channel_opmod(local_chan_p, local_chan_p, channel, "TEST", "Hello World!");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_ME_ID " NOTICE @" TEST_CHANNEL " :<LChanPeon:" TEST_CHANNEL "> Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// Moderated channel
	channel->mode.mode |= MODE_MODERATED;

	sendto_channel_opmod(local_chan_p, local_chan_p, channel, "TEST", "Hello World!");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_ME_ID "90004 TEST @" TEST_CHANNEL " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW | CAP_EOPMOD
	channel->mode.mode &= ~MODE_MODERATED;
	standard_server_caps(CAP_CHW | CAP_EOPMOD, 0);

	sendto_channel_opmod(local_chan_p, local_chan_p, channel, "TEST", "Hello World!");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_ME_ID "90004 TEST =" TEST_CHANNEL " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	ConfigChannel.opmod_send_statusmsg = false;
	standard_free();
}

static void sendto_channel_opmod__local__tags(void)
{
	standard_init();

	strcpy(local_chan_p->user->suser, "test");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;

	// This function does not support TS5...
	standard_ids();

	// Without CAP_CHW | CAP_EOPMOD
	standard_server_caps(0, CAP_CHW | CAP_EOPMOD);

	sendto_channel_opmod(local_chan_p, local_chan_p, channel, "TEST", "Hello World!");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test :LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "No users to receive message; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW, without CAP_EOPMOD
	standard_server_caps(CAP_CHW, CAP_EOPMOD);

	sendto_channel_opmod(local_chan_p, local_chan_p, channel, "TEST", "Hello World!");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test :LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_ME_ID " NOTICE @" TEST_CHANNEL " :<LChanPeon:" TEST_CHANNEL "> Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// Moderated channel
	channel->mode.mode |= MODE_MODERATED;

	local_chan_o->localClient->caps &= ~CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps &= ~CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_ov->localClient->caps &= ~CAP_SERVER_TIME;

	sendto_channel_opmod(local_chan_p, local_chan_p, channel, "TEST", "Hello World!");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("@account=test :LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_ME_ID "90004 TEST " TEST_CHANNEL " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW | CAP_EOPMOD
	channel->mode.mode &= ~MODE_MODERATED;
	standard_server_caps(CAP_CHW | CAP_EOPMOD, 0);

	sendto_channel_opmod(local_chan_p, local_chan_p, channel, "TEST", "Hello World!");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("@account=test :LChanPeon" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Message source; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_ME_ID "90004 TEST =" TEST_CHANNEL " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	standard_free();
}

static void sendto_channel_opmod__remote(void)
{
	standard_init();

	// This function does not support TS5...
	standard_ids();

	// Without CAP_CHW | CAP_EOPMOD
	standard_server_caps(0, CAP_CHW | CAP_EOPMOD);

	sendto_channel_opmod(server2, remote2_chan_d, channel, "TEST", "Hello World!");
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "Message source; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW, without CAP_EOPMOD
	standard_server_caps(CAP_CHW, CAP_EOPMOD);

	sendto_channel_opmod(server2, remote2_chan_d, channel, "TEST", "Hello World!");
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_SERVER2_ID " NOTICE @" TEST_CHANNEL " :<R2ChanDeaf:" TEST_CHANNEL "> Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "Message source; " MSG);

	// Moderated channel
	channel->mode.mode |= MODE_MODERATED;

	sendto_channel_opmod(server2, remote2_chan_d, channel, "TEST", "Hello World!");
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_SERVER2_ID "90205 TEST " TEST_CHANNEL " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "Message source; " MSG);

	// With CAP_CHW | CAP_EOPMOD
	channel->mode.mode &= ~MODE_MODERATED;
	standard_server_caps(CAP_CHW | CAP_EOPMOD, 0);

	sendto_channel_opmod(server2, remote2_chan_d, channel, "TEST", "Hello World!");
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_SERVER2_ID "90205 TEST =" TEST_CHANNEL " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "Message source; " MSG);

	standard_free();
}

static void sendto_channel_opmod_statusmsg__remote(void)
{
	standard_init();
	ConfigChannel.opmod_send_statusmsg = true;

	// This function does not support TS5...
	standard_ids();

	// Without CAP_CHW | CAP_EOPMOD
	standard_server_caps(0, CAP_CHW | CAP_EOPMOD);

	sendto_channel_opmod(server2, remote2_chan_d, channel, "TEST", "Hello World!");
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "Message source; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW, without CAP_EOPMOD
	standard_server_caps(CAP_CHW, CAP_EOPMOD);

	sendto_channel_opmod(server2, remote2_chan_d, channel, "TEST", "Hello World!");
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_SERVER2_ID " NOTICE @" TEST_CHANNEL " :<R2ChanDeaf:" TEST_CHANNEL "> Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "Message source; " MSG);

	// Moderated channel
	channel->mode.mode |= MODE_MODERATED;

	sendto_channel_opmod(server2, remote2_chan_d, channel, "TEST", "Hello World!");
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_SERVER2_ID "90205 TEST @" TEST_CHANNEL " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "Message source; " MSG);

	// With CAP_CHW | CAP_EOPMOD
	channel->mode.mode &= ~MODE_MODERATED;
	standard_server_caps(CAP_CHW | CAP_EOPMOD, 0);

	sendto_channel_opmod(server2, remote2_chan_d, channel, "TEST", "Hello World!");
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST @" TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_SERVER2_ID "90205 TEST =" TEST_CHANNEL " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "Message source; " MSG);

	ConfigChannel.opmod_send_statusmsg = false;
	standard_free();
}

static void sendto_channel_opmod__remote__tags(void)
{
	standard_init();

	strcpy(remote2_chan_d->user->suser, "test");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;

	// This function does not support TS5...
	standard_ids();

	// Without CAP_CHW | CAP_EOPMOD
	standard_server_caps(0, CAP_CHW | CAP_EOPMOD);

	sendto_channel_opmod(server2, remote2_chan_d, channel, "TEST", "Hello World!");
	is_client_sendq("@account=test :R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq_empty(server, "Message source; " MSG);
	is_client_sendq_empty(server2, "No users to receive message; " MSG);

	// With CAP_CHW, without CAP_EOPMOD
	standard_server_caps(CAP_CHW, CAP_EOPMOD);

	sendto_channel_opmod(server2, remote2_chan_d, channel, "TEST", "Hello World!");
	is_client_sendq("@account=test :R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_SERVER2_ID " NOTICE @" TEST_CHANNEL " :<R2ChanDeaf:" TEST_CHANNEL "> Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "Message source; " MSG);

	// Moderated channel
	channel->mode.mode |= MODE_MODERATED;

	local_chan_o->localClient->caps &= ~CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps &= ~CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_ov->localClient->caps &= ~CAP_SERVER_TIME;

	sendto_channel_opmod(server2, remote2_chan_d, channel, "TEST", "Hello World!");
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("@account=test :R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_SERVER2_ID "90205 TEST " TEST_CHANNEL " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "Message source; " MSG);

	// With CAP_CHW | CAP_EOPMOD
	channel->mode.mode &= ~MODE_MODERATED;
	standard_server_caps(CAP_CHW | CAP_EOPMOD, 0);

	sendto_channel_opmod(server2, remote2_chan_d, channel, "TEST", "Hello World!");
	is_client_sendq(":R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("@account=test :R2ChanDeaf" TEST_ID_SUFFIX " TEST " TEST_CHANNEL " :Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Deaf; " MSG);
	is_client_sendq(":" TEST_SERVER2_ID "90205 TEST =" TEST_CHANNEL " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, "Message source; " MSG);

	standard_free();
}

static void sendto_channel_local1(void)
{
	standard_init();

	sendto_channel_local(user, ALL_MEMBERS, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local(user, CHFL_VOICE, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local(user, CHFL_CHANOP, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local(user, CHFL_CHANOP | CHFL_VOICE, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o/+v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_priv(user, ALL_MEMBERS, "test:test", channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not an oper; " MSG);
	is_client_sendq_empty(local_chan_o, "Not an oper; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not an oper; " MSG);
	is_client_sendq_empty(local_chan_v, "Not an oper; " MSG);
	is_client_sendq_empty(local_chan_p, "Not an oper; " MSG);
	is_client_sendq_empty(local_chan_d, "Not an oper; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	struct Client *oper1 = make_local_person_nick("oper1");
	struct Client *oper2 = make_local_person_nick("oper2");

	make_local_person_oper(oper1);
	make_local_person_oper(oper2);

	oper2->umodes |= UMODE_ADMIN;

	add_user_to_channel(lchannel, oper1, CHFL_PEON);
	add_user_to_channel(lchannel, oper2, CHFL_PEON);

	sendto_channel_local(user, ALL_MEMBERS, lchannel, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, user, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, oper1, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, oper2, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_channel_local_priv(user, ALL_MEMBERS, "test:test", lchannel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not an oper; " MSG);
	is_client_sendq("Hello World!" CRLF, oper1, "Is an oper; " MSG);
	is_client_sendq("Hello World!" CRLF, oper2, "Is an oper; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	standard_free();
}

static void sendto_channel_local1__tags(void)
{
	standard_init();

	strcpy(user->user->suser, "test");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_channel_local(user, ALL_MEMBERS, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("@account=test Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local(user, CHFL_VOICE, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq("@account=test Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local(user, CHFL_CHANOP, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local(user, CHFL_CHANOP | CHFL_VOICE, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq("@account=test Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o/+v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_priv(user, ALL_MEMBERS, "test:test", channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not an oper; " MSG);
	is_client_sendq_empty(local_chan_o, "Not an oper; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not an oper; " MSG);
	is_client_sendq_empty(local_chan_v, "Not an oper; " MSG);
	is_client_sendq_empty(local_chan_p, "Not an oper; " MSG);
	is_client_sendq_empty(local_chan_d, "Not an oper; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	struct Client *oper1 = make_local_person_nick("oper1");
	struct Client *oper2 = make_local_person_nick("oper2");

	make_local_person_oper(oper1);
	make_local_person_oper(oper2);

	oper2->umodes |= UMODE_ADMIN;

	add_user_to_channel(lchannel, oper1, CHFL_PEON);
	add_user_to_channel(lchannel, oper2, CHFL_PEON);

	oper1->localClient->caps |= CAP_ACCOUNT_TAG;
	oper2->localClient->caps |= CAP_SERVER_TIME;

	sendto_channel_local(user, ALL_MEMBERS, lchannel, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, user, "On channel; " MSG);
	is_client_sendq("@account=test Hello World!" CRLF, oper1, "On channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, oper2, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_channel_local_priv(user, ALL_MEMBERS, "test:test", lchannel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not an oper; " MSG);
	is_client_sendq("@account=test Hello World!" CRLF, oper1, "Is an oper; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, oper2, "Is an oper; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	oper1->localClient->caps &= ~CAP_ACCOUNT_TAG;
	oper2->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_channel_local(user, ALL_MEMBERS, lchannel, "Hello %s!", "World");
	is_client_sendq("Hello World!" CRLF, user, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, oper1, "On channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test Hello World!" CRLF, oper2, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_channel_local_priv(user, ALL_MEMBERS, "test:test", lchannel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not an oper; " MSG);
	is_client_sendq("Hello World!" CRLF, oper1, "Is an oper; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test Hello World!" CRLF, oper2, "Is an oper; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	standard_free();
}

static void sendto_channel_local_with_capability1(void)
{
	standard_init();

	local_chan_o->localClient->caps |= CAP_MULTI_PREFIX;
	local_chan_v->localClient->caps |= CAP_MULTI_PREFIX;

	sendto_channel_local_with_capability(user, ALL_MEMBERS, CAP_MULTI_PREFIX, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, ALL_MEMBERS, 0, CAP_MULTI_PREFIX, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, ALL_MEMBERS, 0, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_VOICE, CAP_MULTI_PREFIX, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_VOICE, 0, CAP_MULTI_PREFIX, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_VOICE, 0, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_CHANOP, CAP_MULTI_PREFIX, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_CHANOP, 0, CAP_MULTI_PREFIX, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_CHANOP, 0, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_CHANOP | CHFL_VOICE, CAP_MULTI_PREFIX, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_CHANOP | CHFL_VOICE, 0, CAP_MULTI_PREFIX, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o/+v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_CHANOP | CHFL_VOICE, 0, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o/+v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_channel_local_with_capability1__tags(void)
{
	standard_init();

	local_chan_o->localClient->caps |= CAP_MULTI_PREFIX;
	local_chan_v->localClient->caps |= CAP_MULTI_PREFIX;

	strcpy(user->user->suser, "test");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_channel_local_with_capability(user, ALL_MEMBERS, CAP_MULTI_PREFIX, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("@account=test Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, ALL_MEMBERS, 0, CAP_MULTI_PREFIX, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Has cap; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, ALL_MEMBERS, 0, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("@account=test Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_VOICE, CAP_MULTI_PREFIX, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("@account=test Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_VOICE, 0, CAP_MULTI_PREFIX, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_VOICE, 0, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq("@account=test Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_CHANOP, CAP_MULTI_PREFIX, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_CHANOP, 0, CAP_MULTI_PREFIX, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Has cap; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_CHANOP, 0, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_CHANOP | CHFL_VOICE, CAP_MULTI_PREFIX, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("@account=test Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_CHANOP | CHFL_VOICE, 0, CAP_MULTI_PREFIX, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Has cap; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o/+v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability(user, CHFL_CHANOP | CHFL_VOICE, 0, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq("@account=test Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o/+v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_channel_local_with_capability_butone1(void)
{
	standard_init();

	local_chan_o->localClient->caps |= CAP_MULTI_PREFIX;
	local_chan_v->localClient->caps |= CAP_MULTI_PREFIX;

	sendto_channel_local_with_capability_butone(NULL, ALL_MEMBERS, CAP_MULTI_PREFIX, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(NULL, ALL_MEMBERS, 0, CAP_MULTI_PREFIX, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(NULL, ALL_MEMBERS, 0, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(local_chan_o, ALL_MEMBERS, CAP_MULTI_PREFIX, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(local_chan_o, ALL_MEMBERS, 0, CAP_MULTI_PREFIX, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(local_chan_o, ALL_MEMBERS, 0, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(local_chan_p, ALL_MEMBERS, CAP_MULTI_PREFIX, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Is the one (neo); " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(local_chan_p, ALL_MEMBERS, 0, CAP_MULTI_PREFIX, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_p, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(local_chan_p, ALL_MEMBERS, 0, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_channel_local_with_capability_butone1__tags(void)
{
	standard_init();

	local_chan_o->localClient->caps |= CAP_MULTI_PREFIX;
	local_chan_v->localClient->caps |= CAP_MULTI_PREFIX;

	strcpy(local_chan_o->user->suser, "test_o");
	strcpy(local_chan_p->user->suser, "test_p");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_channel_local_with_capability_butone(NULL, ALL_MEMBERS, CAP_MULTI_PREFIX, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(NULL, ALL_MEMBERS, 0, CAP_MULTI_PREFIX, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Has cap; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(NULL, ALL_MEMBERS, 0, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(local_chan_o, ALL_MEMBERS, CAP_MULTI_PREFIX, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("@account=test_o Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(local_chan_o, ALL_MEMBERS, 0, CAP_MULTI_PREFIX, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(local_chan_o, ALL_MEMBERS, 0, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("@account=test_o Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(local_chan_p, ALL_MEMBERS, CAP_MULTI_PREFIX, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_p Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("@account=test_p Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Is the one (neo); " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(local_chan_p, ALL_MEMBERS, 0, CAP_MULTI_PREFIX, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Has cap; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_p, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_with_capability_butone(local_chan_p, ALL_MEMBERS, 0, 0, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_p Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("@account=test_p Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_channel_local_butone1(void)
{
	standard_init();

	sendto_channel_local_butone(NULL, ALL_MEMBERS, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_o, ALL_MEMBERS, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_p, ALL_MEMBERS, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_ov, CHFL_VOICE, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq_empty(local_chan_ov, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_p, CHFL_VOICE, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_o, CHFL_CHANOP, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_p, CHFL_CHANOP, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_v, CHFL_CHANOP | CHFL_VOICE, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_v, "Is the one (neo); " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o/+v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_p, CHFL_CHANOP | CHFL_VOICE, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o/+v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_channel_local_butone1__tags(void)
{
	standard_init();

	strcpy(local_chan_o->user->suser, "test_o");
	strcpy(local_chan_ov->user->suser, "test_ov");
	strcpy(local_chan_v->user->suser, "test_v");
	strcpy(local_chan_p->user->suser, "test_p");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_channel_local_butone(NULL, ALL_MEMBERS, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_o, ALL_MEMBERS, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("@account=test_o Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_p, ALL_MEMBERS, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_p Hello World!" CRLF, local_chan_o, "On channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "On channel; " MSG);
	is_client_sendq("@account=test_p Hello World!" CRLF, local_chan_v, "On channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_ov, CHFL_VOICE, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq_empty(local_chan_ov, "Is the one (neo); " MSG);
	is_client_sendq("@account=test_ov Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_p, CHFL_VOICE, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not +v; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Has +v; " MSG);
	is_client_sendq("@account=test_p Hello World!" CRLF, local_chan_v, "Has +v; " MSG);
	is_client_sendq_empty(local_chan_p, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_o, CHFL_CHANOP, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_p, CHFL_CHANOP, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_p Hello World!" CRLF, local_chan_o, "Has +o; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Has +o; " MSG);
	is_client_sendq_empty(local_chan_v, "Not +o; " MSG);
	is_client_sendq_empty(local_chan_p, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_v, CHFL_CHANOP | CHFL_VOICE, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_v Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_v, "Is the one (neo); " MSG);
	is_client_sendq_empty(local_chan_p, "Not +o/+v; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o/+v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_channel_local_butone(local_chan_p, CHFL_CHANOP | CHFL_VOICE, channel, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_p Hello World!" CRLF, local_chan_o, "Has +o/+v; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Has +o/+v; " MSG);
	is_client_sendq("@account=test_p Hello World!" CRLF, local_chan_v, "Has +o/+v; " MSG);
	is_client_sendq_empty(local_chan_p, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Has +o/+v; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_common_channels_local1(void)
{
	standard_init();

	local_chan_o->localClient->caps |= CAP_MULTI_PREFIX;
	local_chan_v->localClient->caps |= CAP_MULTI_PREFIX;

	sendto_common_channels_local(local_chan_o, CAP_MULTI_PREFIX, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_no_chan, "Not on common channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local(local_chan_o, 0, CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_no_chan, "Not on common channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local(local_chan_o, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "No cap checking; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "On common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Not on common channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local(local_no_chan, CAP_MULTI_PREFIX, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local(local_no_chan, 0, CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_no_chan, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local(local_no_chan, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_no_chan, "No cap checking; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	local_no_chan->localClient->caps |= CAP_MULTI_PREFIX;

	sendto_common_channels_local(local_no_chan, CAP_MULTI_PREFIX, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_no_chan, "Has cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local(local_no_chan, 0, CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Has cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local(local_no_chan, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_no_chan, "No cap checking; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_common_channels_local1__tags(void)
{
	standard_init();

	local_chan_o->localClient->caps |= CAP_MULTI_PREFIX;
	local_chan_v->localClient->caps |= CAP_MULTI_PREFIX;

	strcpy(local_chan_o->user->suser, "test_o");
	strcpy(local_no_chan->user->suser, "test_n");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_common_channels_local(local_chan_o, CAP_MULTI_PREFIX, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_o Hello World!" CRLF, local_chan_o, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("@account=test_o Hello World!" CRLF, local_chan_v, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_no_chan, "Not on common channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local(local_chan_o, 0, CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Has cap; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_no_chan, "Not on common channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local(local_chan_o, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_o Hello World!" CRLF, local_chan_o, "No cap checking; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "On common channel; " MSG);
	is_client_sendq("@account=test_o Hello World!" CRLF, local_chan_v, "On common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Not on common channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	local_no_chan->localClient->caps |= CAP_SERVER_TIME;

	sendto_common_channels_local(local_no_chan, CAP_MULTI_PREFIX, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local(local_no_chan, 0, CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_no_chan, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local(local_no_chan, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_no_chan, "No cap checking; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	local_no_chan->localClient->caps |= CAP_MULTI_PREFIX;
	local_no_chan->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_common_channels_local(local_no_chan, CAP_MULTI_PREFIX, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_n Hello World!" CRLF, local_no_chan, "Has cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local(local_no_chan, 0, CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Has cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local(local_no_chan, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_n Hello World!" CRLF, local_no_chan, "No cap checking; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	local_no_chan->localClient->caps &= ~CAP_SERVER_TIME;

	sendto_common_channels_local(local_no_chan, CAP_MULTI_PREFIX, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq("@account=test_n Hello World!" CRLF, local_no_chan, "Has cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local(local_no_chan, 0, CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Has cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local(local_no_chan, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq("@account=test_n Hello World!" CRLF, local_no_chan, "No cap checking; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_common_channels_local_butone1(void)
{
	standard_init();

	local_chan_o->localClient->caps |= CAP_MULTI_PREFIX;
	local_chan_v->localClient->caps |= CAP_MULTI_PREFIX;

	sendto_common_channels_local_butone(local_chan_o, CAP_MULTI_PREFIX, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_no_chan, "Not on common channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local_butone(local_chan_o, 0, CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_no_chan, "Not on common channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local_butone(local_chan_o, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_ov, "On common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "On common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Not on common channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local_butone(local_no_chan, CAP_MULTI_PREFIX, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Is the one (neo); " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local_butone(local_no_chan, 0, CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Is the one (neo); " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local_butone(local_no_chan, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Is the one (neo); " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	local_no_chan->localClient->caps |= CAP_MULTI_PREFIX;

	sendto_common_channels_local_butone(local_no_chan, CAP_MULTI_PREFIX, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Is the one (neo); " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local_butone(local_no_chan, 0, CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Is the one (neo); " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local_butone(local_no_chan, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Is the one (neo); " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_common_channels_local_butone1__tags(void)
{
	standard_init();

	local_chan_o->localClient->caps |= CAP_MULTI_PREFIX;
	local_chan_v->localClient->caps |= CAP_MULTI_PREFIX;

	strcpy(local_chan_o->user->suser, "test_o");
	strcpy(local_no_chan->user->suser, "test_n");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_common_channels_local_butone(local_chan_o, CAP_MULTI_PREFIX, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("@account=test_o Hello World!" CRLF, local_chan_v, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_no_chan, "Not on common channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local_butone(local_chan_o, 0, CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_v, "Has cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_no_chan, "Not on common channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local_butone(local_chan_o, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Is the one (neo); " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "On common channel; " MSG);
	is_client_sendq("@account=test_o Hello World!" CRLF, local_chan_v, "On common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_p, "On common channel; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_d, "On common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Not on common channel; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	local_no_chan->localClient->caps |= CAP_SERVER_TIME;

	sendto_common_channels_local_butone(local_no_chan, CAP_MULTI_PREFIX, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Is the one (neo); " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local_butone(local_no_chan, 0, CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Is the one (neo); " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local_butone(local_no_chan, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Is the one (neo); " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	local_no_chan->localClient->caps |= CAP_MULTI_PREFIX;
	local_no_chan->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_common_channels_local_butone(local_no_chan, CAP_MULTI_PREFIX, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Is the one (neo); " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local_butone(local_no_chan, 0, CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Is the one (neo); " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local_butone(local_no_chan, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Is the one (neo); " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	local_no_chan->localClient->caps &= ~CAP_SERVER_TIME;

	sendto_common_channels_local_butone(local_no_chan, CAP_MULTI_PREFIX, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Is the one (neo); " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local_butone(local_no_chan, 0, CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Is the one (neo); " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_common_channels_local_butone(local_no_chan, 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_o, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_p, "Not on common channel; " MSG);
	is_client_sendq_empty(local_chan_d, "Not on common channel; " MSG);
	is_client_sendq_empty(local_no_chan, "Is the one (neo); " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_match_butone__host(void)
{
	standard_init();

	// This function does not support TS5...
	standard_ids();

	// Local
	sendto_match_butone(NULL, user, "*.test", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Host matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Host matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Host matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Host matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Host matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Host matches; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(NULL, user, "*.invalid", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, user, "*.test", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Host matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Host matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Host matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Host matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Host matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Host matches; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, user, "*.invalid", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	// Remote
	sendto_match_butone(NULL, remote, "*.test", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(NULL, remote, "*.invalid", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, remote, "*.test", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Host matches; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, remote, "*.invalid", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	standard_free();
}

static void sendto_match_butone__host__tags(void)
{
	standard_init();

	strcpy(user->user->suser, "test");
	strcpy(remote->user->suser, "rtest");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	// This function does not support TS5...
	standard_ids();

	// Local
	sendto_match_butone(NULL, user, "*.test", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Host matches; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test :" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Host matches; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Host matches; " MSG);
	is_client_sendq("@account=test :" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Host matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Host matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Host matches; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(NULL, user, "*.invalid", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, user, "*.test", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Host matches; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test :" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Host matches; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Host matches; " MSG);
	is_client_sendq("@account=test :" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Host matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Host matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Host matches; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, user, "*.invalid", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	// Remote
	sendto_match_butone(NULL, remote, "*.test", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Host matches; " MSG);
	is_client_sendq("@account=rtest :" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Host matches; " MSG);
	is_client_sendq("@account=rtest :" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(NULL, remote, "*.invalid", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, remote, "*.test", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Host matches; " MSG);
	is_client_sendq("@account=rtest :" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Host matches; " MSG);
	is_client_sendq("@account=rtest :" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Host matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Host matches; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, remote, "*.invalid", MATCH_HOST, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	standard_free();
}

static void sendto_match_butone__server(void)
{
	standard_init();

	// This function does not support TS5...
	standard_ids();

	// Local
	sendto_match_butone(NULL, user, "me.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Server matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Server matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Server matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Server matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Server matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Server matches; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(NULL, user, "example.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, user, "me.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Server matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Server matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Server matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Server matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Server matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Server matches; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, user, "example.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	// Remote
	sendto_match_butone(NULL, remote, "me.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(NULL, remote, "example.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, remote, "me.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Server matches; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, remote, "example.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	standard_free();
}

static void sendto_match_butone__server__tags(void)
{
	standard_init();

	strcpy(user->user->suser, "test");
	strcpy(remote->user->suser, "rtest");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	// This function does not support TS5...
	standard_ids();

	// Local
	sendto_match_butone(NULL, user, "me.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Server matches; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test :" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Server matches; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Server matches; " MSG);
	is_client_sendq("@account=test :" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Server matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Server matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Server matches; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(NULL, user, "example.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, user, "me.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Server matches; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test :" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Server matches; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Server matches; " MSG);
	is_client_sendq("@account=test :" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Server matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Server matches; " MSG);
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Server matches; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, user, "example.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	// Remote
	sendto_match_butone(NULL, remote, "me.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Server matches; " MSG);
	is_client_sendq("@account=rtest :" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Server matches; " MSG);
	is_client_sendq("@account=rtest :" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(NULL, remote, "example.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, remote, "me.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, user, "Server matches; " MSG);
	is_client_sendq("@account=rtest :" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_o, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_ov, "Server matches; " MSG);
	is_client_sendq("@account=rtest :" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_v, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_p, "Server matches; " MSG);
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST Hello World!" CRLF, local_chan_d, "Server matches; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	sendto_match_butone(server, remote, "example.*", MATCH_SERVER, "TEST Hello %s!", "World");
	is_client_sendq_empty(user, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_o, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_ov, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_v, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_p, "Host doesn't match; " MSG);
	is_client_sendq_empty(local_chan_d, "Host doesn't match; " MSG);
	is_client_sendq_empty(server, "Is the one (neo); " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server2, "Is a server; " MSG);
	is_client_sendq(":" TEST_REMOTE_ID " TEST Hello World!" CRLF, server3, "Is a server; " MSG);

	standard_free();
}

static void sendto_local_clients_with_capability1(void)
{
	standard_init();

	local_chan_o->localClient->caps |= CAP_MULTI_PREFIX;
	local_chan_v->localClient->caps |= CAP_MULTI_PREFIX;

	sendto_local_clients_with_capability(CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_local_clients_with_capability1__tags(void)
{
	standard_init();

	local_chan_o->localClient->caps |= CAP_MULTI_PREFIX;
	local_chan_v->localClient->caps |= CAP_MULTI_PREFIX;

	strcpy(user->user->suser, "test");
	strcpy(local_chan_o->user->suser, "test_o");
	strcpy(local_chan_ov->user->suser, "test_ov");
	strcpy(local_chan_v->user->suser, "test_v");
	strcpy(local_chan_p->user->suser, "test_p");
	strcpy(local_chan_d->user->suser, "test_d");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_local_clients_with_capability(CAP_MULTI_PREFIX, "Hello %s!", "World");
	is_client_sendq_empty(user, "Doesn't have cap; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_o, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_ov, "Doesn't have cap; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "Has cap; " MSG);
	is_client_sendq_empty(local_chan_p, "Doesn't have cap; " MSG);
	is_client_sendq_empty(local_chan_d, "Doesn't have cap; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_monitor1(void)
{
	struct monitor *monptr;

	standard_init();

	monptr = find_monitor(TEST_NICK, 1);
	rb_dlinkAddAlloc(local_chan_o, &monptr->users);
	rb_dlinkAddAlloc(monptr, &local_chan_o->localClient->monitor_list);
	rb_dlinkAddAlloc(local_chan_v, &monptr->users);
	rb_dlinkAddAlloc(monptr, &local_chan_v->localClient->monitor_list);

	sendto_monitor(user, monptr, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not monitoring; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_o, "Monitoring; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not monitoring; " MSG);
	is_client_sendq("Hello World!" CRLF, local_chan_v, "Monitoring; " MSG);
	is_client_sendq_empty(local_chan_p, "Not monitoring; " MSG);
	is_client_sendq_empty(local_chan_d, "Not monitoring; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_monitor1__tags(void)
{
	struct monitor *monptr;

	standard_init();

	strcpy(user->user->suser, "test");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	monptr = find_monitor(TEST_NICK, 1);
	rb_dlinkAddAlloc(local_chan_o, &monptr->users);
	rb_dlinkAddAlloc(monptr, &local_chan_o->localClient->monitor_list);
	rb_dlinkAddAlloc(local_chan_v, &monptr->users);
	rb_dlinkAddAlloc(monptr, &local_chan_v->localClient->monitor_list);

	sendto_monitor(user, monptr, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not monitoring; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test Hello World!" CRLF, local_chan_o, "Monitoring; " MSG);
	is_client_sendq_empty(local_chan_ov, "Not monitoring; " MSG);
	is_client_sendq("@account=test Hello World!" CRLF, local_chan_v, "Monitoring; " MSG);
	is_client_sendq_empty(local_chan_p, "Not monitoring; " MSG);
	is_client_sendq_empty(local_chan_d, "Not monitoring; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	rb_dlinkAddAlloc(local_chan_ov, &monptr->users);
	rb_dlinkAddAlloc(monptr, &local_chan_ov->localClient->monitor_list);
	clear_monitor(local_chan_o);
	clear_monitor(local_chan_v);

	sendto_monitor(user, monptr, "Hello %s!", "World");
	is_client_sendq_empty(user, "Not monitoring; " MSG);
	is_client_sendq_empty(local_chan_o, "Not monitoring; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " Hello World!" CRLF, local_chan_ov, "Monitoring; " MSG);
	is_client_sendq_empty(local_chan_v, "Not monitoring; " MSG);
	is_client_sendq_empty(local_chan_p, "Not monitoring; " MSG);
	is_client_sendq_empty(local_chan_d, "Not monitoring; " MSG);
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_anywhere1(void)
{
	standard_init();

	// This function does not support TS5...
	standard_ids();

	// Local
	sendto_anywhere(user, remote, "TEST", "Hello %s!", "World");
	is_client_sendq(":" TEST_REMOTE_NICK TEST_ID_SUFFIX " TEST " TEST_NICK " Hello World!" CRLF, user, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere(user, local_chan_o, "TEST", "Hello %s!", "World");
	is_client_sendq(":LChanOp" TEST_ID_SUFFIX " TEST " TEST_NICK " Hello World!" CRLF, user, MSG);
	is_client_sendq_empty(local_chan_o, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere(user, server, "TEST", "Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER_NAME " TEST " TEST_NICK " Hello World!" CRLF, user, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere(user, &me, "TEST", "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " TEST " TEST_NICK " Hello World!" CRLF, user, MSG);
	is_client_sendq_empty(server, MSG);

	// Remote
	sendto_anywhere(remote, user, "TEST", "Hello %s!", "World");
	is_client_sendq_empty(user, MSG);
	is_client_sendq(":" TEST_ID " TEST " TEST_REMOTE_ID " Hello World!" CRLF, server, MSG);

	sendto_anywhere(remote, local_chan_o, "TEST", "Hello %s!", "World");
	is_client_sendq_empty(local_chan_o, MSG);
	is_client_sendq(":" TEST_ME_ID "90001 TEST " TEST_REMOTE_ID " Hello World!" CRLF, server, MSG);

	sendto_anywhere(remote, server2, "TEST", "Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER2_ID " TEST " TEST_REMOTE_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_anywhere(remote, &me, "TEST", "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " TEST " TEST_REMOTE_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_anywhere1__tags(void)
{
	standard_init();

	strcpy(user->user->suser, "test");
	strcpy(local_chan_o->user->suser, "test_o");
	strcpy(local_chan_ov->user->suser, "test_ov");
	strcpy(local_chan_v->user->suser, "test_v");
	strcpy(local_chan_p->user->suser, "test_p");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	// This function does not support TS5...
	standard_ids();

	// Local
	sendto_anywhere(user, local_chan_p, "TEST", "Hello %s!", "World");
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST " TEST_NICK " Hello World!" CRLF, user, MSG);
	is_client_sendq_empty(local_chan_p, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere(local_chan_o, local_chan_p, "TEST", "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_p :LChanPeon" TEST_ID_SUFFIX " TEST LChanOp Hello World!" CRLF, local_chan_o, MSG);
	is_client_sendq_empty(local_chan_p, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere(local_chan_ov, local_chan_p, "TEST", "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :LChanPeon" TEST_ID_SUFFIX " TEST LChanOpVoice Hello World!" CRLF, local_chan_ov, MSG);
	is_client_sendq_empty(local_chan_p, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere(local_chan_v, local_chan_p, "TEST", "Hello %s!", "World");
	is_client_sendq("@account=test_p :LChanPeon" TEST_ID_SUFFIX " TEST LChanVoice Hello World!" CRLF, local_chan_v, MSG);
	is_client_sendq_empty(local_chan_p, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere(user, server, "TEST", "Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER_NAME " TEST " TEST_NICK " Hello World!" CRLF, user, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere(user, &me, "TEST", "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " TEST " TEST_NICK " Hello World!" CRLF, user, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere(local_chan_o, server, "TEST", "Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER_NAME " TEST LChanOp Hello World!" CRLF, local_chan_o, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere(local_chan_v, &me, "TEST", "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " TEST LChanVoice Hello World!" CRLF, local_chan_v, MSG);
	is_client_sendq_empty(server, MSG);

	// Remote
	sendto_anywhere(remote, user, "TEST", "Hello %s!", "World");
	is_client_sendq_empty(user, MSG);
	is_client_sendq(":" TEST_ID " TEST " TEST_REMOTE_ID " Hello World!" CRLF, server, MSG);

	sendto_anywhere(remote, local_chan_o, "TEST", "Hello %s!", "World");
	is_client_sendq_empty(local_chan_o, MSG);
	is_client_sendq(":" TEST_ME_ID "90001 TEST " TEST_REMOTE_ID " Hello World!" CRLF, server, MSG);

	sendto_anywhere(remote, local_chan_ov, "TEST", "Hello %s!", "World");
	is_client_sendq_empty(local_chan_ov, MSG);
	is_client_sendq(":" TEST_ME_ID "90002 TEST " TEST_REMOTE_ID " Hello World!" CRLF, server, MSG);

	sendto_anywhere(remote, local_chan_v, "TEST", "Hello %s!", "World");
	is_client_sendq_empty(local_chan_v, MSG);
	is_client_sendq(":" TEST_ME_ID "90003 TEST " TEST_REMOTE_ID " Hello World!" CRLF, server, MSG);

	sendto_anywhere(remote, server2, "TEST", "Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER2_ID " TEST " TEST_REMOTE_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_anywhere(remote, &me, "TEST", "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " TEST " TEST_REMOTE_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void sendto_anywhere_echo1(void)
{
	standard_init();

	// Local
	sendto_anywhere_echo(user, user, "TEST", "Hello %s!", "World");
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST " TEST_NICK " Hello World!" CRLF, user, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(user, local_chan_o, "TEST", "Hello %s!", "World");
	is_client_sendq_empty(user, MSG);
	is_client_sendq(":LChanOp" TEST_ID_SUFFIX " TEST " TEST_NICK " Hello World!" CRLF, local_chan_o, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(&me, user, "TEST", "Hello %s!", "World");
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST " TEST_ME_NAME " Hello World!" CRLF, user, MSG);
	is_client_sendq_empty(server, MSG);

	// Remote
	sendto_anywhere_echo(remote, user, "TEST", "Hello %s!", "World");
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST " TEST_REMOTE_NICK " Hello World!" CRLF, user, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(server, user, "TEST", "Hello %s!", "World");
	is_client_sendq(":" TEST_NICK TEST_ID_SUFFIX " TEST " TEST_SERVER_NAME " Hello World!" CRLF, user, MSG);
	is_client_sendq_empty(server, MSG);

	standard_free();
}

static void sendto_anywhere_echo1__tags(void)
{
	standard_init();

	strcpy(user->user->suser, "test");
	strcpy(local_chan_o->user->suser, "test_o");
	strcpy(local_chan_ov->user->suser, "test_ov");
	strcpy(local_chan_v->user->suser, "test_v");
	strcpy(local_chan_p->user->suser, "test_p");
	local_chan_o->localClient->caps |= CAP_ACCOUNT_TAG;
	local_chan_o->localClient->caps |= CAP_SERVER_TIME;
	local_chan_ov->localClient->caps |= CAP_SERVER_TIME;
	local_chan_v->localClient->caps |= CAP_ACCOUNT_TAG;

	sendto_anywhere_echo(local_chan_o, local_chan_o, "TEST", "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_o :LChanOp" TEST_ID_SUFFIX " TEST LChanOp Hello World!" CRLF, local_chan_o, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(user, local_chan_o, "TEST", "Hello %s!", "World");
	is_client_sendq_empty(user, MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_o :LChanOp" TEST_ID_SUFFIX " TEST " TEST_NICK " Hello World!" CRLF, local_chan_o, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(local_chan_p, local_chan_o, "TEST", "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_o :LChanOp" TEST_ID_SUFFIX " TEST LChanPeon Hello World!" CRLF, local_chan_o, MSG);
	is_client_sendq_empty(local_chan_p, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(&me, local_chan_o, "TEST", "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_o :LChanOp" TEST_ID_SUFFIX " TEST " TEST_ME_NAME " Hello World!" CRLF, local_chan_o, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(remote, local_chan_o, "TEST", "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_o :LChanOp" TEST_ID_SUFFIX " TEST " TEST_REMOTE_NICK " Hello World!" CRLF, local_chan_o, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(server, local_chan_o, "TEST", "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test_o :LChanOp" TEST_ID_SUFFIX " TEST " TEST_SERVER_NAME " Hello World!" CRLF, local_chan_o, MSG);
	is_client_sendq_empty(server, MSG);


	sendto_anywhere_echo(local_chan_ov, local_chan_ov, "TEST", "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :LChanOpVoice" TEST_ID_SUFFIX " TEST LChanOpVoice Hello World!" CRLF, local_chan_ov, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(user, local_chan_ov, "TEST", "Hello %s!", "World");
	is_client_sendq_empty(user, MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :LChanOpVoice" TEST_ID_SUFFIX " TEST " TEST_NICK " Hello World!" CRLF, local_chan_ov, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(local_chan_p, local_chan_ov, "TEST", "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :LChanOpVoice" TEST_ID_SUFFIX " TEST LChanPeon Hello World!" CRLF, local_chan_ov, MSG);
	is_client_sendq_empty(local_chan_p, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(&me, local_chan_ov, "TEST", "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :LChanOpVoice" TEST_ID_SUFFIX " TEST " TEST_ME_NAME " Hello World!" CRLF, local_chan_ov, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(remote, local_chan_ov, "TEST", "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :LChanOpVoice" TEST_ID_SUFFIX " TEST " TEST_REMOTE_NICK " Hello World!" CRLF, local_chan_ov, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(server, local_chan_ov, "TEST", "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :LChanOpVoice" TEST_ID_SUFFIX " TEST " TEST_SERVER_NAME " Hello World!" CRLF, local_chan_ov, MSG);
	is_client_sendq_empty(server, MSG);


	sendto_anywhere_echo(local_chan_v, local_chan_v, "TEST", "Hello %s!", "World");
	is_client_sendq("@account=test_v :LChanVoice" TEST_ID_SUFFIX " TEST LChanVoice Hello World!" CRLF, local_chan_v, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(user, local_chan_v, "TEST", "Hello %s!", "World");
	is_client_sendq_empty(user, MSG);
	is_client_sendq("@account=test_v :LChanVoice" TEST_ID_SUFFIX " TEST " TEST_NICK " Hello World!" CRLF, local_chan_v, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(local_chan_p, local_chan_v, "TEST", "Hello %s!", "World");
	is_client_sendq("@account=test_v :LChanVoice" TEST_ID_SUFFIX " TEST LChanPeon Hello World!" CRLF, local_chan_v, MSG);
	is_client_sendq_empty(local_chan_p, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(&me, local_chan_v, "TEST", "Hello %s!", "World");
	is_client_sendq("@account=test_v :LChanVoice" TEST_ID_SUFFIX " TEST " TEST_ME_NAME " Hello World!" CRLF, local_chan_v, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(remote, local_chan_v, "TEST", "Hello %s!", "World");
	is_client_sendq("@account=test_v :LChanVoice" TEST_ID_SUFFIX " TEST " TEST_REMOTE_NICK " Hello World!" CRLF, local_chan_v, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(server, local_chan_v, "TEST", "Hello %s!", "World");
	is_client_sendq("@account=test_v :LChanVoice" TEST_ID_SUFFIX " TEST " TEST_SERVER_NAME " Hello World!" CRLF, local_chan_v, MSG);
	is_client_sendq_empty(server, MSG);


	sendto_anywhere_echo(local_chan_p, local_chan_p, "TEST", "Hello %s!", "World");
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST LChanPeon Hello World!" CRLF, local_chan_p, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(user, local_chan_p, "TEST", "Hello %s!", "World");
	is_client_sendq_empty(user, MSG);
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST " TEST_NICK " Hello World!" CRLF, local_chan_p, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(&me, local_chan_p, "TEST", "Hello %s!", "World");
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST " TEST_ME_NAME " Hello World!" CRLF, local_chan_p, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(remote, local_chan_p, "TEST", "Hello %s!", "World");
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST " TEST_REMOTE_NICK " Hello World!" CRLF, local_chan_p, MSG);
	is_client_sendq_empty(server, MSG);

	sendto_anywhere_echo(server, local_chan_p, "TEST", "Hello %s!", "World");
	is_client_sendq(":LChanPeon" TEST_ID_SUFFIX " TEST " TEST_SERVER_NAME " Hello World!" CRLF, local_chan_p, MSG);
	is_client_sendq_empty(server, MSG);

	standard_free();
}

static void sendto_match_servs1(void)
{
	standard_init();

	server->localClient->caps = CAP_ENCAP;
	server2->localClient->caps = CAP_ENCAP;
	server2->localClient->caps |= CAP_KNOCK;
	server3->localClient->caps = CAP_BAN;

	// This function does not support TS5...
	standard_ids();

	// Match all
	sendto_match_servs(&me, "*.test", 0, 0, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server3, MSG);

	sendto_match_servs(user, "*.test", 0, 0, "Hello %s!", "World");
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server3, MSG);

	sendto_match_servs(remote, "*.test", 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_REMOTE_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_REMOTE_ID " Hello World!" CRLF, server3, MSG);

	sendto_match_servs(server, "*.test", 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_SERVER_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_SERVER_ID " Hello World!" CRLF, server3, MSG);

	// Match all, CAP_ENCAP but not CAP_KNOCK
	sendto_match_servs(&me, "*.test", CAP_ENCAP, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(user, "*.test", CAP_ENCAP, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(remote, "*.test", CAP_ENCAP, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(server, "*.test", CAP_ENCAP, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	// Match all, but not CAP_BAN
	sendto_match_servs(&me, "*.test", 0, CAP_BAN, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(user, "*.test", 0, CAP_BAN, "Hello %s!", "World");
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(remote, "*.test", 0, CAP_BAN, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_REMOTE_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(server, "*.test", 0, CAP_BAN, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_SERVER_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	// Match all, CAP_BAN but not CAP_KNOCK
	sendto_match_servs(&me, "*.test", CAP_BAN, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server3, MSG);

	sendto_match_servs(user, "*.test", CAP_BAN, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server3, MSG);

	sendto_match_servs(remote, "*.test", CAP_BAN, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq(":" TEST_REMOTE_ID " Hello World!" CRLF, server3, MSG);

	sendto_match_servs(server, "*.test", CAP_BAN, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq(":" TEST_SERVER_ID " Hello World!" CRLF, server3, MSG);

	// Match all, CAP_KNOCK
	sendto_match_servs(&me, "*.test", CAP_KNOCK, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(user, "*.test", CAP_KNOCK, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(remote, "*.test", CAP_KNOCK, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_REMOTE_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(server, "*.test", CAP_KNOCK, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_SERVER_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	// Match none
	sendto_match_servs(&me, "*.invalid", 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(user, "*.invalid", 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(remote, "*.invalid", 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(server, "*.invalid", 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	standard_free();
}

static void sendto_match_servs1__tags(void)
{
	standard_init();

	strcpy(user->user->suser, "test");
	strcpy(remote->user->suser, "rtest");
	user->localClient->caps |= CAP_ACCOUNT_TAG;
	user->localClient->caps |= CAP_SERVER_TIME;

	server->localClient->caps = CAP_ENCAP;
	server2->localClient->caps = CAP_ENCAP;
	server2->localClient->caps |= CAP_KNOCK;
	server3->localClient->caps = CAP_BAN;

	// This function does not support TS5...
	standard_ids();

	// Match all
	sendto_match_servs(&me, "*.test", 0, 0, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server3, MSG);

	sendto_match_servs(user, "*.test", 0, 0, "Hello %s!", "World");
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server3, MSG);

	sendto_match_servs(remote, "*.test", 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_REMOTE_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_REMOTE_ID " Hello World!" CRLF, server3, MSG);

	sendto_match_servs(server, "*.test", 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_SERVER_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_SERVER_ID " Hello World!" CRLF, server3, MSG);

	// Match all, CAP_ENCAP but not CAP_KNOCK
	sendto_match_servs(&me, "*.test", CAP_ENCAP, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(user, "*.test", CAP_ENCAP, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(remote, "*.test", CAP_ENCAP, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(server, "*.test", CAP_ENCAP, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	// Match all, but not CAP_BAN
	sendto_match_servs(&me, "*.test", 0, CAP_BAN, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(user, "*.test", 0, CAP_BAN, "Hello %s!", "World");
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(remote, "*.test", 0, CAP_BAN, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_REMOTE_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(server, "*.test", 0, CAP_BAN, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_SERVER_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	// Match all, CAP_BAN but not CAP_KNOCK
	sendto_match_servs(&me, "*.test", CAP_BAN, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server3, MSG);

	sendto_match_servs(user, "*.test", CAP_BAN, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server3, MSG);

	sendto_match_servs(remote, "*.test", CAP_BAN, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq(":" TEST_REMOTE_ID " Hello World!" CRLF, server3, MSG);

	sendto_match_servs(server, "*.test", CAP_BAN, CAP_KNOCK, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq(":" TEST_SERVER_ID " Hello World!" CRLF, server3, MSG);

	// Match all, CAP_KNOCK
	sendto_match_servs(&me, "*.test", CAP_KNOCK, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_ME_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(user, "*.test", CAP_KNOCK, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(remote, "*.test", CAP_KNOCK, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_REMOTE_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(server, "*.test", CAP_KNOCK, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq(":" TEST_SERVER_ID " Hello World!" CRLF, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	// Match none
	sendto_match_servs(&me, "*.invalid", 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(user, "*.invalid", 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(remote, "*.invalid", 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	sendto_match_servs(server, "*.invalid", 0, 0, "Hello %s!", "World");
	is_client_sendq_empty(server, MSG);
	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	standard_free();
}

static void sendto_realops_snomask1(void)
{
	struct Client *oper1 = make_local_person_nick("oper1");
	struct Client *oper2 = make_local_person_nick("oper2");
	struct Client *oper3 = make_local_person_nick("oper3");
	struct Client *oper4 = make_local_person_nick("oper4");

	standard_init();

	make_local_person_oper(oper1);
	make_local_person_oper(oper2);
	make_local_person_oper(oper3);
	make_local_person_oper(oper4);

	oper1->snomask = SNO_BOTS | SNO_SKILL;
	oper2->snomask = SNO_GENERAL | SNO_REJ;
	oper3->snomask = SNO_BOTS | SNO_SKILL;
	oper4->snomask = SNO_GENERAL | SNO_REJ;

	oper3->user->privset = privilegeset_get("admin");
	oper4->user->privset = privilegeset_get("admin");

	server->localClient->caps = CAP_ENCAP | CAP_TS6;
	server2->localClient->caps = 0;

	ConfigFileEntry.global_snotices = 0;
	remote_rehash_oper_p = NULL;

	sendto_realops_snomask(SNO_BOTS, L_ALL, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask(SNO_BOTS, L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask(SNO_BOTS, L_OPER, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper3, "Not an oper; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper2, "Not an admin; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_OPER, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper4, "Not an oper; " MSG);
	is_client_sendq_empty(server, MSG);

	remote_rehash_oper_p = remote;

	sendto_realops_snomask(SNO_BOTS, L_ALL, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE " TEST_REMOTE_NICK " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_BOTS, L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE " TEST_REMOTE_NICK " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_BOTS, L_OPER, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper3, "Not an oper; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE " TEST_REMOTE_NICK " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE " TEST_REMOTE_NICK " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper2, "Not an admin; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE " TEST_REMOTE_NICK " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_OPER, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper4, "Not an oper; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE " TEST_REMOTE_NICK " :*** Notice -- Hello World!" CRLF, server, MSG);

	standard_ids();

	sendto_realops_snomask(SNO_BOTS, L_ALL, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " NOTICE " TEST_REMOTE_ID " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_BOTS, L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " NOTICE " TEST_REMOTE_ID " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_BOTS, L_OPER, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper3, "Not an oper; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " NOTICE " TEST_REMOTE_ID " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " NOTICE " TEST_REMOTE_ID " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper2, "Not an admin; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " NOTICE " TEST_REMOTE_ID " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_OPER, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper4, "Not an oper; " MSG);
	is_client_sendq(":" TEST_ME_ID " NOTICE " TEST_REMOTE_ID " :*** Notice -- Hello World!" CRLF, server, MSG);

	// This feature does not support TS5...
	ConfigFileEntry.global_snotices = 1;

	sendto_realops_snomask(SNO_BOTS, L_NETWIDE | L_ALL, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " ENCAP * SNOTE b :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_realops_snomask(SNO_BOTS, L_NETWIDE | L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " ENCAP * SNOTE b :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_realops_snomask(SNO_BOTS, L_NETWIDE | L_OPER, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper3, "Not an oper; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " ENCAP * SNOTE b :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE | L_ALL, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " ENCAP * SNOTE s :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE | L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper2, "Not an admin; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " ENCAP * SNOTE s :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE | L_OPER, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper4, "Not an oper; " MSG);
	is_client_sendq(":" TEST_ME_ID " ENCAP * SNOTE s :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	remove_local_person(oper1);
	remove_local_person(oper2);
	remove_local_person(oper3);
	remove_local_person(oper4);

	standard_free();
}

static void sendto_realops_snomask1__tags(void)
{
	struct Client *oper1 = make_local_person_nick("oper1");
	struct Client *oper2 = make_local_person_nick("oper2");
	struct Client *oper3 = make_local_person_nick("oper3");
	struct Client *oper4 = make_local_person_nick("oper4");

	standard_init();

	strcpy(oper1->user->suser, "test1");
	strcpy(oper2->user->suser, "test2");
	strcpy(oper3->user->suser, "test3");
	strcpy(oper4->user->suser, "test4");

	oper1->localClient->caps |= CAP_ACCOUNT_TAG;
	oper1->localClient->caps |= CAP_SERVER_TIME;
	oper2->localClient->caps |= CAP_SERVER_TIME;
	oper3->localClient->caps |= CAP_ACCOUNT_TAG;

	make_local_person_oper(oper1);
	make_local_person_oper(oper2);
	make_local_person_oper(oper3);
	make_local_person_oper(oper4);

	oper1->snomask = SNO_BOTS | SNO_SKILL;
	oper2->snomask = SNO_GENERAL | SNO_REJ;
	oper3->snomask = SNO_BOTS | SNO_SKILL;
	oper4->snomask = SNO_GENERAL | SNO_REJ;

	oper3->user->privset = privilegeset_get("admin");
	oper4->user->privset = privilegeset_get("admin");

	server->localClient->caps = CAP_ENCAP | CAP_TS6;
	server2->localClient->caps = 0;

	ConfigFileEntry.global_snotices = 0;
	remote_rehash_oper_p = NULL;

	sendto_realops_snomask(SNO_BOTS, L_ALL, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask(SNO_BOTS, L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask(SNO_BOTS, L_OPER, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper3, "Not an oper; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper2, "Not an admin; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_OPER, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper4, "Not an oper; " MSG);
	is_client_sendq_empty(server, MSG);

	remote_rehash_oper_p = remote;

	sendto_realops_snomask(SNO_BOTS, L_ALL, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE " TEST_REMOTE_NICK " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_BOTS, L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE " TEST_REMOTE_NICK " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_BOTS, L_OPER, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper3, "Not an oper; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE " TEST_REMOTE_NICK " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE " TEST_REMOTE_NICK " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper2, "Not an admin; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE " TEST_REMOTE_NICK " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_OPER, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper4, "Not an oper; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE " TEST_REMOTE_NICK " :*** Notice -- Hello World!" CRLF, server, MSG);

	standard_ids();

	sendto_realops_snomask(SNO_BOTS, L_ALL, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " NOTICE " TEST_REMOTE_ID " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_BOTS, L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " NOTICE " TEST_REMOTE_ID " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_BOTS, L_OPER, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper3, "Not an oper; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " NOTICE " TEST_REMOTE_ID " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_ALL, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " NOTICE " TEST_REMOTE_ID " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper2, "Not an admin; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " NOTICE " TEST_REMOTE_ID " :*** Notice -- Hello World!" CRLF, server, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_OPER, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper4, "Not an oper; " MSG);
	is_client_sendq(":" TEST_ME_ID " NOTICE " TEST_REMOTE_ID " :*** Notice -- Hello World!" CRLF, server, MSG);

	// This feature does not support TS5...
	ConfigFileEntry.global_snotices = 1;

	sendto_realops_snomask(SNO_BOTS, L_NETWIDE | L_ALL, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " ENCAP * SNOTE b :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_realops_snomask(SNO_BOTS, L_NETWIDE | L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " ENCAP * SNOTE b :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_realops_snomask(SNO_BOTS, L_NETWIDE | L_OPER, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper3, "Not an oper; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " ENCAP * SNOTE b :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE | L_ALL, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " ENCAP * SNOTE s :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE | L_ADMIN, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper2, "Not an admin; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq(":" TEST_ME_ID " ENCAP * SNOTE s :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE | L_OPER, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper4, "Not an oper; " MSG);
	is_client_sendq(":" TEST_ME_ID " ENCAP * SNOTE s :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	remove_local_person(oper1);
	remove_local_person(oper2);
	remove_local_person(oper3);
	remove_local_person(oper4);

	standard_free();
}

static void sendto_realops_snomask_from1(void)
{
	struct Client *oper1 = make_local_person_nick("oper1");
	struct Client *oper2 = make_local_person_nick("oper2");
	struct Client *oper3 = make_local_person_nick("oper3");
	struct Client *oper4 = make_local_person_nick("oper4");

	make_local_person_oper(oper1);
	make_local_person_oper(oper2);
	make_local_person_oper(oper3);
	make_local_person_oper(oper4);

	oper1->snomask = SNO_BOTS | SNO_SKILL;
	oper2->snomask = SNO_GENERAL | SNO_REJ;
	oper3->snomask = SNO_BOTS | SNO_SKILL;
	oper4->snomask = SNO_GENERAL | SNO_REJ;

	oper3->user->privset = privilegeset_get("admin");
	oper4->user->privset = privilegeset_get("admin");

	sendto_realops_snomask_from(SNO_BOTS, L_ALL, &me, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_BOTS, L_ADMIN, &me, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_BOTS, L_OPER, &me, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper3, "Not an oper; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_BOTS, L_ALL, server, "Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_BOTS, L_ADMIN, server, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_BOTS, L_OPER, server, "Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper3, "Not an oper; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_GENERAL, L_ALL, &me, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_GENERAL, L_ADMIN, &me, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper2, "Not an admin; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_GENERAL, L_OPER, &me, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper4, "Not an oper; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_GENERAL, L_ALL, server, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_GENERAL, L_ADMIN, server, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper2, "Not an admin; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_GENERAL, L_OPER, server, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper4, "Not an oper; " MSG);
	is_client_sendq_empty(server, MSG);

	remove_local_person(oper1);
	remove_local_person(oper2);
	remove_local_person(oper3);
	remove_local_person(oper4);
}

static void sendto_realops_snomask_from1__tags(void)
{
	struct Client *oper1 = make_local_person_nick("oper1");
	struct Client *oper2 = make_local_person_nick("oper2");
	struct Client *oper3 = make_local_person_nick("oper3");
	struct Client *oper4 = make_local_person_nick("oper4");

	strcpy(oper1->user->suser, "test1");
	strcpy(oper2->user->suser, "test2");
	strcpy(oper3->user->suser, "test3");
	strcpy(oper4->user->suser, "test4");

	oper1->localClient->caps |= CAP_ACCOUNT_TAG;
	oper1->localClient->caps |= CAP_SERVER_TIME;
	oper2->localClient->caps |= CAP_SERVER_TIME;
	oper3->localClient->caps |= CAP_ACCOUNT_TAG;

	make_local_person_oper(oper1);
	make_local_person_oper(oper2);
	make_local_person_oper(oper3);
	make_local_person_oper(oper4);

	oper1->snomask = SNO_BOTS | SNO_SKILL;
	oper2->snomask = SNO_GENERAL | SNO_REJ;
	oper3->snomask = SNO_BOTS | SNO_SKILL;
	oper4->snomask = SNO_GENERAL | SNO_REJ;

	oper3->user->privset = privilegeset_get("admin");
	oper4->user->privset = privilegeset_get("admin");

	sendto_realops_snomask_from(SNO_BOTS, L_ALL, &me, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_BOTS, L_ADMIN, &me, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_BOTS, L_OPER, &me, "Hello %s!", "World");
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper3, "Not an oper; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_BOTS, L_ALL, server, "Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_BOTS, L_ADMIN, server, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper3, "Matches mask; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_BOTS, L_OPER, server, "Hello %s!", "World");
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper1, "Matches mask; " MSG);
	is_client_sendq_empty(oper2, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper3, "Not an oper; " MSG);
	is_client_sendq_empty(oper4, "Doesn't match mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_GENERAL, L_ALL, &me, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_GENERAL, L_ADMIN, &me, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper2, "Not an admin; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_GENERAL, L_OPER, &me, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper4, "Not an oper; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_GENERAL, L_ALL, server, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_GENERAL, L_ADMIN, server, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper2, "Not an admin; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper4, "Matches mask; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_realops_snomask_from(SNO_GENERAL, L_OPER, server, "Hello %s!", "World");
	is_client_sendq_empty(oper1, "Doesn't match mask; " MSG);
	is_client_sendq(":" TEST_SERVER_NAME " NOTICE * :*** Notice -- Hello World!" CRLF, oper2, "Matches mask; " MSG);
	is_client_sendq_empty(oper3, "Doesn't match mask; " MSG);
	is_client_sendq_empty(oper4, "Not an oper; " MSG);
	is_client_sendq_empty(server, MSG);

	remove_local_person(oper1);
	remove_local_person(oper2);
	remove_local_person(oper3);
	remove_local_person(oper4);
}

static void sendto_wallops_flags1(void)
{
	struct Client *user1 = make_local_person_nick("user1");
	struct Client *user2 = make_local_person_nick("user2");
	struct Client *oper1 = make_local_person_nick("oper1");
	struct Client *oper2 = make_local_person_nick("oper2");
	struct Client *oper3 = make_local_person_nick("oper3");
	struct Client *oper4 = make_local_person_nick("oper4");

	make_local_person_oper(oper1);
	make_local_person_oper(oper2);
	make_local_person_oper(oper3);
	make_local_person_oper(oper4);

	user1->umodes |= UMODE_WALLOP;
	oper1->umodes |= UMODE_WALLOP | UMODE_OPERWALL;
	oper2->umodes |= UMODE_WALLOP | UMODE_OPERWALL | UMODE_ADMIN;
	oper3->umodes |= UMODE_WALLOP;
	oper4->umodes |= UMODE_OPERWALL;

	sendto_wallops_flags(UMODE_WALLOP, oper1, "Test to users");
	is_client_sendq(":oper1" TEST_ID_SUFFIX " WALLOPS :Test to users" CRLF, user1, "User is +w; " MSG);
	is_client_sendq_empty(user2, "User is -w; " MSG);
	is_client_sendq(":oper1" TEST_ID_SUFFIX " WALLOPS :Test to users" CRLF, oper1, "User is +w; " MSG);
	is_client_sendq(":oper1" TEST_ID_SUFFIX " WALLOPS :Test to users" CRLF, oper2, "User is +w; " MSG);
	is_client_sendq(":oper1" TEST_ID_SUFFIX " WALLOPS :Test to users" CRLF, oper3, "User is +w; " MSG);
	is_client_sendq_empty(oper4, "User is -w; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_wallops_flags(UMODE_OPERWALL, oper2, "Test to opers");
	is_client_sendq_empty(user1, "Not an oper; " MSG);
	is_client_sendq_empty(user2, "Not an oper; " MSG);
	is_client_sendq(":oper2" TEST_ID_SUFFIX " WALLOPS :Test to opers" CRLF, oper1, "Oper is +z; " MSG);
	is_client_sendq(":oper2" TEST_ID_SUFFIX " WALLOPS :Test to opers" CRLF, oper2, "Oper is +z; " MSG);
	is_client_sendq_empty(oper3, "Oper is -z; " MSG);
	is_client_sendq(":oper2" TEST_ID_SUFFIX " WALLOPS :Test to opers" CRLF, oper4, "Oper is +z; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_wallops_flags(UMODE_ADMIN, &me, "Test to admins");
	is_client_sendq_empty(user1, "Not an admin; " MSG);
	is_client_sendq_empty(user2, "Not an admin; " MSG);
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq(":" TEST_ME_NAME " WALLOPS :Test to admins" CRLF, oper2, MSG);
	is_client_sendq_empty(oper3, "Not an admin; " MSG);
	is_client_sendq_empty(oper4, "Not an admin; " MSG);
	is_client_sendq_empty(server, MSG);

	remove_local_person(user1);
	remove_local_person(user2);
	remove_local_person(oper1);
	remove_local_person(oper2);
	remove_local_person(oper3);
	remove_local_person(oper4);
}

static void sendto_wallops_flags1__tags(void)
{
	struct Client *user1 = make_local_person_nick("user1");
	struct Client *user2 = make_local_person_nick("user2");
	struct Client *oper1 = make_local_person_nick("oper1");
	struct Client *oper2 = make_local_person_nick("oper2");
	struct Client *oper3 = make_local_person_nick("oper3");
	struct Client *oper4 = make_local_person_nick("oper4");

	strcpy(oper1->user->suser, "test1");
	strcpy(oper2->user->suser, "test2");
	strcpy(oper3->user->suser, "test3");
	strcpy(oper4->user->suser, "test4");

	oper1->localClient->caps |= CAP_ACCOUNT_TAG;
	oper1->localClient->caps |= CAP_SERVER_TIME;
	oper2->localClient->caps |= CAP_SERVER_TIME;
	oper3->localClient->caps |= CAP_ACCOUNT_TAG;

	make_local_person_oper(oper1);
	make_local_person_oper(oper2);
	make_local_person_oper(oper3);
	make_local_person_oper(oper4);

	user1->umodes |= UMODE_WALLOP;
	oper1->umodes |= UMODE_WALLOP | UMODE_OPERWALL;
	oper2->umodes |= UMODE_WALLOP | UMODE_OPERWALL | UMODE_ADMIN;
	oper3->umodes |= UMODE_WALLOP;
	oper4->umodes |= UMODE_OPERWALL;

	sendto_wallops_flags(UMODE_WALLOP, oper1, "Test to users");
	is_client_sendq(":oper1" TEST_ID_SUFFIX " WALLOPS :Test to users" CRLF, user1, "User is +w; " MSG);
	is_client_sendq_empty(user2, "User is -w; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test1 :oper1" TEST_ID_SUFFIX " WALLOPS :Test to users" CRLF, oper1, "User is +w; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :oper1" TEST_ID_SUFFIX " WALLOPS :Test to users" CRLF, oper2, "User is +w; " MSG);
	is_client_sendq("@account=test1 :oper1" TEST_ID_SUFFIX " WALLOPS :Test to users" CRLF, oper3, "User is +w; " MSG);
	is_client_sendq_empty(oper4, "User is -w; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_wallops_flags(UMODE_OPERWALL, oper2, "Test to opers");
	is_client_sendq_empty(user1, "Not an oper; " MSG);
	is_client_sendq_empty(user2, "Not an oper; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test2 :oper2" TEST_ID_SUFFIX " WALLOPS :Test to opers" CRLF, oper1, "Oper is +z; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :oper2" TEST_ID_SUFFIX " WALLOPS :Test to opers" CRLF, oper2, "Oper is +z; " MSG);
	is_client_sendq_empty(oper3, "Oper is -z; " MSG);
	is_client_sendq(":oper2" TEST_ID_SUFFIX " WALLOPS :Test to opers" CRLF, oper4, "Oper is +z; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_wallops_flags(UMODE_ADMIN, &me, "Test to admins");
	is_client_sendq_empty(user1, "Not an admin; " MSG);
	is_client_sendq_empty(user2, "Not an admin; " MSG);
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " WALLOPS :Test to admins" CRLF, oper2, MSG);
	is_client_sendq_empty(oper3, "Not an admin; " MSG);
	is_client_sendq_empty(oper4, "Not an admin; " MSG);
	is_client_sendq_empty(server, MSG);

	remove_local_person(user1);
	remove_local_person(user2);
	remove_local_person(oper1);
	remove_local_person(oper2);
	remove_local_person(oper3);
	remove_local_person(oper4);
}

static void sendto_wallops_flags2(void)
{
	struct Client *user1 = make_local_person_nick("user1");
	struct Client *user2 = make_local_person_nick("user2");
	struct Client *oper1 = make_local_person_nick("oper1");
	struct Client *oper2 = make_local_person_nick("oper2");
	struct Client *oper3 = make_local_person_nick("oper3");
	struct Client *oper4 = make_local_person_nick("oper4");

	make_local_person_oper(oper1);
	make_local_person_oper(oper2);
	make_local_person_oper(oper3);
	make_local_person_oper(oper4);

	user1->umodes |= UMODE_WALLOP;
	oper1->umodes |= UMODE_WALLOP | UMODE_OPERWALL;
	oper2->umodes |= UMODE_WALLOP | UMODE_OPERWALL | UMODE_ADMIN;
	oper3->umodes |= UMODE_WALLOP;
	oper4->umodes |= UMODE_OPERWALL;

	sendto_wallops_flags(UMODE_WALLOP, oper1, "Test to users %s", "42");
	is_client_sendq(":oper1" TEST_ID_SUFFIX " WALLOPS :Test to users 42" CRLF, user1, "User is +w; " MSG);
	is_client_sendq_empty(user2, "User is -w; " MSG);
	is_client_sendq(":oper1" TEST_ID_SUFFIX " WALLOPS :Test to users 42" CRLF, oper1, "User is +w; " MSG);
	is_client_sendq(":oper1" TEST_ID_SUFFIX " WALLOPS :Test to users 42" CRLF, oper2, "User is +w; " MSG);
	is_client_sendq(":oper1" TEST_ID_SUFFIX " WALLOPS :Test to users 42" CRLF, oper3, "User is +w; " MSG);
	is_client_sendq_empty(oper4, "User is -w; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_wallops_flags(UMODE_OPERWALL, oper2, "Test to opers %s", "42");
	is_client_sendq_empty(user1, "Not an oper; " MSG);
	is_client_sendq_empty(user2, "Not an oper; " MSG);
	is_client_sendq(":oper2" TEST_ID_SUFFIX " WALLOPS :Test to opers 42" CRLF, oper1, "Oper is +z; " MSG);
	is_client_sendq(":oper2" TEST_ID_SUFFIX " WALLOPS :Test to opers 42" CRLF, oper2, "Oper is +z; " MSG);
	is_client_sendq_empty(oper3, "Oper is -z; " MSG);
	is_client_sendq(":oper2" TEST_ID_SUFFIX " WALLOPS :Test to opers 42" CRLF, oper4, "Oper is +z; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_wallops_flags(UMODE_ADMIN, &me, "Test to admins %s", "42");
	is_client_sendq_empty(user1, "Not an admin; " MSG);
	is_client_sendq_empty(user2, "Not an admin; " MSG);
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq(":" TEST_ME_NAME " WALLOPS :Test to admins 42" CRLF, oper2, MSG);
	is_client_sendq_empty(oper3, "Not an admin; " MSG);
	is_client_sendq_empty(oper4, "Not an admin; " MSG);
	is_client_sendq_empty(server, MSG);

	remove_local_person(user1);
	remove_local_person(user2);
	remove_local_person(oper1);
	remove_local_person(oper2);
	remove_local_person(oper3);
	remove_local_person(oper4);
}

static void sendto_wallops_flags2__tags(void)
{
	struct Client *user1 = make_local_person_nick("user1");
	struct Client *user2 = make_local_person_nick("user2");
	struct Client *oper1 = make_local_person_nick("oper1");
	struct Client *oper2 = make_local_person_nick("oper2");
	struct Client *oper3 = make_local_person_nick("oper3");
	struct Client *oper4 = make_local_person_nick("oper4");

	strcpy(oper1->user->suser, "test1");
	strcpy(oper2->user->suser, "test2");
	strcpy(oper3->user->suser, "test3");
	strcpy(oper4->user->suser, "test4");

	oper1->localClient->caps |= CAP_ACCOUNT_TAG;
	oper1->localClient->caps |= CAP_SERVER_TIME;
	oper2->localClient->caps |= CAP_SERVER_TIME;
	oper3->localClient->caps |= CAP_ACCOUNT_TAG;

	make_local_person_oper(oper1);
	make_local_person_oper(oper2);
	make_local_person_oper(oper3);
	make_local_person_oper(oper4);

	user1->umodes |= UMODE_WALLOP;
	oper1->umodes |= UMODE_WALLOP | UMODE_OPERWALL;
	oper2->umodes |= UMODE_WALLOP | UMODE_OPERWALL | UMODE_ADMIN;
	oper3->umodes |= UMODE_WALLOP;
	oper4->umodes |= UMODE_OPERWALL;

	sendto_wallops_flags(UMODE_WALLOP, oper1, "Test to users %s", "42");
	is_client_sendq(":oper1" TEST_ID_SUFFIX " WALLOPS :Test to users 42" CRLF, user1, "User is +w; " MSG);
	is_client_sendq_empty(user2, "User is -w; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test1 :oper1" TEST_ID_SUFFIX " WALLOPS :Test to users 42" CRLF, oper1, "User is +w; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :oper1" TEST_ID_SUFFIX " WALLOPS :Test to users 42" CRLF, oper2, "User is +w; " MSG);
	is_client_sendq("@account=test1 :oper1" TEST_ID_SUFFIX " WALLOPS :Test to users 42" CRLF, oper3, "User is +w; " MSG);
	is_client_sendq_empty(oper4, "User is -w; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_wallops_flags(UMODE_OPERWALL, oper2, "Test to opers %s", "42");
	is_client_sendq_empty(user1, "Not an oper; " MSG);
	is_client_sendq_empty(user2, "Not an oper; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME ";account=test2 :oper2" TEST_ID_SUFFIX " WALLOPS :Test to opers 42" CRLF, oper1, "Oper is +z; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :oper2" TEST_ID_SUFFIX " WALLOPS :Test to opers 42" CRLF, oper2, "Oper is +z; " MSG);
	is_client_sendq_empty(oper3, "Oper is -z; " MSG);
	is_client_sendq(":oper2" TEST_ID_SUFFIX " WALLOPS :Test to opers 42" CRLF, oper4, "Oper is +z; " MSG);
	is_client_sendq_empty(server, MSG);

	sendto_wallops_flags(UMODE_ADMIN, &me, "Test to admins %s", "42");
	is_client_sendq_empty(user1, "Not an admin; " MSG);
	is_client_sendq_empty(user2, "Not an admin; " MSG);
	is_client_sendq_empty(oper1, "Not an admin; " MSG);
	is_client_sendq("@time=" ADVENTURE_TIME " :" TEST_ME_NAME " WALLOPS :Test to admins 42" CRLF, oper2, MSG);
	is_client_sendq_empty(oper3, "Not an admin; " MSG);
	is_client_sendq_empty(oper4, "Not an admin; " MSG);
	is_client_sendq_empty(server, MSG);

	remove_local_person(user1);
	remove_local_person(user2);
	remove_local_person(oper1);
	remove_local_person(oper2);
	remove_local_person(oper3);
	remove_local_person(oper4);
}

static void kill_client1(void)
{
	standard_init();

	kill_client(server, remote, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " KILL " TEST_REMOTE_NICK " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_ids();

	kill_client(server, remote, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE_ID " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void kill_client1__tags(void)
{
	standard_init();

	strcpy(remote->user->suser, "test");

	kill_client(server, remote, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_NAME " KILL " TEST_REMOTE_NICK " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_ids();

	kill_client(server, remote, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE_ID " :Hello World!" CRLF, server, MSG);
	is_client_sendq_empty(server2, MSG);

	standard_free();
}

static void kill_client_serv_butone1(void)
{
	standard_init();

	// This function does not support TS5...
	standard_ids();

	// If the server being sent to (or the kill target) is TS6,
	// then "but one" is ignored and the kill is sent anyway
	kill_client_serv_butone(remote, remote, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE_ID " :Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE_ID " :Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE_ID " :Hello World!" CRLF, server3, MSG);

	kill_client_serv_butone(remote, remote2, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE2_ID " :Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE2_ID " :Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE2_ID " :Hello World!" CRLF, server3, MSG);

	kill_client_serv_butone(remote2, remote, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE_ID " :Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE_ID " :Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE_ID " :Hello World!" CRLF, server3, MSG);

	kill_client_serv_butone(remote2, remote2, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE2_ID " :Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE2_ID " :Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE2_ID " :Hello World!" CRLF, server3, MSG);

	standard_free();
}

static void kill_client_serv_butone1__tags(void)
{
	standard_init();

	strcpy(remote2->user->suser, "test");
	strcpy(remote2->user->suser, "test2");

	// This function does not support TS5...
	standard_ids();

	// If the server being sent to (or the kill target) is TS6,
	// then "but one" is ignored and the kill is sent anyway
	kill_client_serv_butone(remote, remote, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE_ID " :Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE_ID " :Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE_ID " :Hello World!" CRLF, server3, MSG);

	kill_client_serv_butone(remote, remote2, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE2_ID " :Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE2_ID " :Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE2_ID " :Hello World!" CRLF, server3, MSG);

	kill_client_serv_butone(remote2, remote, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE_ID " :Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE_ID " :Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE_ID " :Hello World!" CRLF, server3, MSG);

	kill_client_serv_butone(remote2, remote2, "Hello %s!", "World");
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE2_ID " :Hello World!" CRLF, server, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE2_ID " :Hello World!" CRLF, server2, MSG);
	is_client_sendq(":" TEST_ME_ID " KILL " TEST_REMOTE2_ID " :Hello World!" CRLF, server3, MSG);

	standard_free();
}

int main(int argc, char *argv[])
{
	plan_lazy();

	ircd_util_init(__FILE__);
	client_util_init();

	// Load modules in a predictable order so that tags are added in the same order every time
	ircd_util_reload_module("cap_account_tag");
	ircd_util_reload_module("cap_server_time");

	CAP_ACCOUNT_TAG = capability_get(cli_capindex, "account-tag", NULL);
	ok(CAP_ACCOUNT_TAG != 0, "CAP_ACCOUNT_TAG missing; " MSG);

	CAP_SERVER_TIME = capability_get(cli_capindex, "server-time", NULL);
	ok(CAP_SERVER_TIME != 0, "CAP_SERVER_TIME missing; " MSG);

	CAP_MULTI_PREFIX = capability_get(cli_capindex, "multi-prefix", NULL);
	ok(CAP_MULTI_PREFIX != 0, "CAP_MULTI_PREFIX missing; " MSG);

	sendto_one1();
	sendto_one1__tags();
	sendto_one_prefix1();
	sendto_one_prefix1__tags();
	sendto_one_notice1();
	sendto_one_notice1__tags();
	sendto_one_numeric1();
	sendto_one_numeric1__tags();
	sendto_server1();
	sendto_server1__tags();

	sendto_channel_flags__local__all_members();
	sendto_channel_flags__remote__all_members();
	sendto_channel_flags__local__all_members__tags();
	sendto_channel_flags__remote__all_members__tags();
	sendto_channel_flags__local__voice();
	sendto_channel_flags__remote__voice();
	sendto_channel_flags__local__chanop();
	sendto_channel_flags__remote__chanop();
	sendto_channel_flags__local__chanop_voice();
	sendto_channel_flags__remote__chanop_voice();

	sendto_channel_opmod__local();
	sendto_channel_opmod_statusmsg__local();
	sendto_channel_opmod__local__tags();
	sendto_channel_opmod__remote();
	sendto_channel_opmod_statusmsg__remote();
	sendto_channel_opmod__remote__tags();
	sendto_channel_local1();
	sendto_channel_local1__tags();
	sendto_channel_local_with_capability1();
	sendto_channel_local_with_capability1__tags();
	sendto_channel_local_with_capability_butone1();
	sendto_channel_local_with_capability_butone1__tags();
	sendto_channel_local_butone1();
	sendto_channel_local_butone1__tags();
	sendto_common_channels_local1();
	sendto_common_channels_local1__tags();
	sendto_common_channels_local_butone1();
	sendto_common_channels_local_butone1__tags();

	sendto_match_butone__host();
	sendto_match_butone__host__tags();
	sendto_match_butone__server();
	sendto_match_butone__server__tags();
	sendto_match_servs1();
	sendto_match_servs1__tags();
	sendto_local_clients_with_capability1();
	sendto_local_clients_with_capability1__tags();
	sendto_monitor1();
	sendto_monitor1__tags();
	sendto_anywhere1();
	sendto_anywhere1__tags();
	sendto_anywhere_echo1();
	sendto_anywhere_echo1__tags();

	sendto_realops_snomask1();
	sendto_realops_snomask1__tags();
	sendto_realops_snomask_from1();
	sendto_realops_snomask_from1__tags();
	sendto_wallops_flags1();
	sendto_wallops_flags1__tags();
	sendto_wallops_flags2();
	sendto_wallops_flags2__tags();

	kill_client1();
	kill_client1__tags();
	kill_client_serv_butone1();
	kill_client_serv_butone1__tags();

	client_util_free();
	ircd_util_free();
	return 0;
}
