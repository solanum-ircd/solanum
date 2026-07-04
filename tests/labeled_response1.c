/*
 * Copyright (c) 2026 Ryan Schmidt <skizzerz@skizzerz.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <math.h>

#include "stdinc.h"
#include "tap/basic.h"
#include "ircd_util.h"
#include "client_util.h"

#include "batch.h"
#include "hash.h"
#include "hostmask.h"
#include "send.h"
#include "s_serv.h"
#include "newconf.h"
#include "response.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_user.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__
#define LONG_VALUE_50 "12345678901234567890123456789012345678901234567890"
#define LONG_VALUE_100 LONG_VALUE_50 LONG_VALUE_50
#define LONG_VALUE_200 LONG_VALUE_100 LONG_VALUE_100

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
static struct Client *local_no_chan;

static struct Client *remote_chan_o;
static struct Client *remote_chan_ov;
static struct Client *remote_chan_v;
static struct Client *remote_chan_p;

static struct Client *remote2_chan_p;
static struct Client *remote2_no_chan;

static char batch1[BATCH_ID_LEN];
static char batch2[BATCH_ID_LEN];
static char batch3[BATCH_ID_LEN];
static char batch4[BATCH_ID_LEN];
static char batch5[BATCH_ID_LEN];
static char batch6[BATCH_ID_LEN];
static char batch7[BATCH_ID_LEN];
static char batch8[BATCH_ID_LEN];

static void standard_init(void)
{
	user = make_local_person_id(TEST_NICK, TEST_ID);
	server = make_remote_server_full(&me, TEST_SERVER_NAME, TEST_SERVER_ID);
	remote = make_remote_person_id(server, TEST_REMOTE_NICK, TEST_REMOTE_ID);
	server2 = make_remote_server_full(&me, TEST_SERVER2_NAME, TEST_SERVER2_ID);
	remote2 = make_remote_person_id(server2, TEST_REMOTE2_NICK, TEST_REMOTE2_ID);
	server3 = make_remote_server_full(&me, TEST_SERVER3_NAME, TEST_SERVER3_ID);
	remote3 = make_remote_person_id(server3, TEST_REMOTE3_NICK, TEST_REMOTE3_ID);

	SetServerCap(server, CAP_STAG | CAP_ENCAP);
	SetServerCap(server2, CAP_STAG | CAP_ENCAP);
	SetServerCap(server3, CAP_STAG | CAP_ENCAP);

	local_chan_o = make_local_person_id("LChanOp", TEST_ME_ID "90001");
	local_chan_ov = make_local_person_id("LChanOpVoice", TEST_ME_ID "90002");
	local_chan_v = make_local_person_id("LChanVoice", TEST_ME_ID "90003");
	local_chan_p = make_local_person_id("LChanPeon", TEST_ME_ID "90004");
	local_no_chan = make_local_person_id("LNoChan", TEST_ME_ID "90005");

	remote_chan_o = make_remote_person_id(server, "RChanOp", TEST_SERVER_ID "90101");
	remote_chan_ov = make_remote_person_id(server, "RChanOpVoice", TEST_SERVER_ID "90102");
	remote_chan_v = make_remote_person_id(server, "RChanVoice", TEST_SERVER_ID "90103");
	remote_chan_p = make_remote_person_id(server, "RChanPeon", TEST_SERVER_ID "90104");

	remote2_chan_p = make_remote_person_id(server2, "R2ChanPeon", TEST_SERVER2_ID "90204");
	remote2_no_chan = make_remote_person_id(server2, "R2NoChan", TEST_SERVER2_ID "90205");

	channel = make_channel();

	add_user_to_channel(channel, local_chan_o, CHFL_CHANOP);
	add_user_to_channel(channel, local_chan_ov, CHFL_CHANOP | CHFL_VOICE);
	add_user_to_channel(channel, local_chan_v, CHFL_VOICE);
	add_user_to_channel(channel, local_chan_p, CHFL_PEON);

	add_user_to_channel(channel, remote_chan_o, CHFL_CHANOP);
	add_user_to_channel(channel, remote_chan_ov, CHFL_CHANOP | CHFL_VOICE);
	add_user_to_channel(channel, remote_chan_v, CHFL_VOICE);
	add_user_to_channel(channel, remote_chan_p, CHFL_PEON);

	add_user_to_channel(channel, remote2_chan_p, CHFL_PEON);

	lchannel = get_or_create_channel(&me, "&test", NULL);

	add_user_to_channel(lchannel, user, CHFL_PEON);
	add_user_to_channel(lchannel, remote, CHFL_PEON);
	add_user_to_channel(lchannel, remote2, CHFL_PEON);
	add_user_to_channel(lchannel, remote3, CHFL_PEON);

	/* for consistent batch IDs */
	srand(0);
}

static void init_large_list(void)
{
	char chname[CHANNELLEN];

	SetClientCap(user, CLICAP_LABELED_RESPONSE | CLICAP_BATCH);
	attach_conf(user, find_conf_by_address(user->host, user->sockhost, NULL,
		(struct sockaddr *)&user->localClient->ip, CONF_CLIENT, GET_SS_FAMILY(&user->localClient->ip),
		user->username, user->localClient->auth_user));

	for (int i = 0; i < 100; i++)
	{
		snprintf(chname, sizeof(chname), "#safelist%02d", i);
		struct Channel *c = get_or_create_channel(user, chname, NULL);
		add_user_to_channel(c, user, CHFL_PEON);
		set_channel_topic(c, LONG_VALUE_200, "test!test@test", 0);
	}
}

static void make_local_person_admin(struct Client *client_p)
{
	struct oper_conf *oper_p = find_oper_conf(client_p->username, client_p->orighost, client_p->sockhost, "admin");
	oper_up(client_p, oper_p);
	drain_client_sendq(client_p);
	drain_client_sendq(server);
	drain_client_sendq(server2);
	drain_client_sendq(server3);
}

static void standard_free(void)
{
	remove_remote_person(remote2_chan_p);
	remove_remote_person(remote2_no_chan);

	remove_remote_person(remote_chan_o);
	remove_remote_person(remote_chan_ov);
	remove_remote_person(remote_chan_v);
	remove_remote_person(remote_chan_p);

	remove_local_person(local_chan_o);
	remove_local_person(local_chan_ov);
	remove_local_person(local_chan_v);
	remove_local_person(local_chan_p);
	remove_local_person(local_no_chan);

	remove_remote_person(remote3);
	remove_remote_server(server3);
	remove_remote_person(remote2);
	remove_remote_server(server2);
	remove_remote_person(remote);
	remove_remote_server(server);

	if (user != NULL)
		remove_local_person(user);
}

static void single_response(void)
{
	char expected[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_LABELED_RESPONSE | CLICAP_BATCH | CLICAP_ECHO_MESSAGE);

	client_util_parse(user, "@label=foo PRIVMSG &test :hello");
	snprintf(expected, sizeof(expected), "@label=foo :%s!%s@%s PRIVMSG &test :hello" CRLF, user->name, user->username, user->host);
	is_client_sendq(expected, user, MSG);

	/* echo-message self-PMs generate the unlabeled delivery first and the labeled echo second */
	client_util_parse(user, "@label=foo PRIVMSG " TEST_NICK " :hello");
	snprintf(expected, sizeof(expected), ":%s!%s@%s PRIVMSG %s :hello" CRLF, user->name, user->username, user->host, user->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@label=foo :%s!%s@%s PRIVMSG %s :hello" CRLF, user->name, user->username, user->host, user->name);
	is_client_sendq(expected, user, MSG);

	ClearClientCap(user, CLICAP_ECHO_MESSAGE);

	/* without echo-message we expect a labeled ACK instead, still after the delivered message */
	client_util_parse(user, "@label=foo PRIVMSG " TEST_NICK " :hello");
	snprintf(expected, sizeof(expected), ":%s!%s@%s PRIVMSG %s :hello" CRLF, user->name, user->username, user->host, user->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@label=foo :%s ACK" CRLF, me.name);
	is_client_sendq(expected, user, MSG);

	client_util_parse(user, "@label=foo PRIVMSG #notachannel :hello");
	snprintf(expected, sizeof(expected), "@label=foo :%s 401 %s #notachannel :No such nick/channel" CRLF, me.name, user->name);
	is_client_sendq(expected, user, MSG);

	/* other (non-PRIVMSG) commands that return single-line responses */
	client_util_parse(user, "@label=foo USERHOST " TEST_NICK);
	snprintf(expected, sizeof(expected), "@label=foo :%s 302 %s :%s=+%s@%s " CRLF, me.name, user->name, user->name, user->username, user->sockhost);
	is_client_sendq(expected, user, MSG);

	client_util_parse(user, "@label=foo TOPIC " TEST_CHANNEL);
	snprintf(expected, sizeof(expected), "@label=foo :%s 331 %s %s :No topic is set." CRLF, me.name, user->name, channel->chname);
	is_client_sendq(expected, user, MSG);

	standard_free();
}

static void single_response__multi_client(void)
{
	char expected[BUFSIZE];
	rb_dictionary *dict = rb_dictionary_get_for_tests("remote labeled responses");

	standard_init();
	SetClientCap(user, CLICAP_LABELED_RESPONSE | CLICAP_BATCH);
	SetClientCap(local_chan_p, CLICAP_LABELED_RESPONSE | CLICAP_BATCH);
	SetClientCap(local_chan_v, CLICAP_LABELED_RESPONSE | CLICAP_BATCH);
	/* remote WHOIS has a server-wide rate limit; ensure we don't trip it */
	make_local_person_admin(local_chan_p);
	make_local_person_admin(local_chan_v);

	client_util_parse(local_chan_p, "@label=foo WHOIS RChanOp RChanOp");
	client_util_parse(local_chan_v, "@label=foo WHOIS RChanOp RChanOp");
	is_int(1, rb_dlink_list_length(&local_chan_p->localClient->pending_remote_responses), MSG);
	is_int(1, rb_dlink_list_length(&local_chan_v->localClient->pending_remote_responses), MSG);
	is_int(2, rb_dictionary_size(dict), MSG);

	client_util_parse(user, "@label=foo PRIVMSG #notachannel :hello");
	snprintf(expected, sizeof(expected), "@label=foo :%s 401 %s #notachannel :No such nick/channel" CRLF, me.name, user->name);
	is_client_sendq(expected, user, MSG);

	is_int(1, rb_dlink_list_length(&local_chan_p->localClient->pending_remote_responses), MSG);
	is_int(1, rb_dlink_list_length(&local_chan_v->localClient->pending_remote_responses), MSG);
	is_int(2, rb_dictionary_size(dict), MSG);

	standard_free();
}

static void multi_response(void)
{
	char expected[BUFSIZE];
	char *line;

	standard_init();
	SetClientCap(user, CLICAP_LABELED_RESPONSE | CLICAP_BATCH);

	client_util_parse(user, "@label=foo CAP LIST");
	snprintf(expected, sizeof(expected), "@label=foo :%s BATCH +%s labeled-response" CRLF, me.name, batch1);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s CAP %s LIST :batch labeled-response" CRLF, batch1, me.name, user->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF, me.name, batch1);
	is_client_sendq(expected, user, MSG);

	client_util_parse(user, "@label=foo LIST >0");
	snprintf(expected, sizeof(expected), "@label=foo :%s BATCH +%s labeled-response" CRLF, me.name, batch2);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s 321 %s Channel :Users  Name" CRLF, batch2, me.name, user->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s 322 %s #test 9 :" CRLF, batch2, me.name, user->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s 322 %s &test 4 :" CRLF, batch2, me.name, user->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s 323 %s :End of /LIST" CRLF, batch2, me.name, user->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF, me.name, batch2);
	is_client_sendq(expected, user, MSG);

	client_util_parse(user, "@label=foo WHOIS " TEST_REMOTE_NICK);
	snprintf(expected, sizeof(expected), "@label=foo :%s BATCH +%s labeled-response" CRLF, me.name, batch3);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s ", batch3);
	while (*(line = get_client_sendq(user)) != '\0')
	{
		if (*line == '@')
		{
			*(line + strlen(expected)) = '\0';
			is_string(expected, line, MSG);
		}
		else
			break;
	}
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF, me.name, batch3);
	is_string(expected, line, MSG);
	is_client_sendq_empty(user, MSG);

	client_util_parse(user, "@label=foo JOIN " TEST_CHANNEL);
	snprintf(expected, sizeof(expected), "@label=foo :%s BATCH +%s labeled-response" CRLF, me.name, batch4);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s ", batch4);
	while (*(line = get_client_sendq(user)) != '\0')
	{
		if (*line == '@')
		{
			*(line + strlen(expected)) = '\0';
			is_string(expected, line, MSG);
		}
		else
			break;
	}
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF, me.name, batch4);
	is_string(expected, line, MSG);
	is_client_sendq_empty(user, MSG);

	client_util_parse(user, "@label=foo WHO " TEST_CHANNEL);
	snprintf(expected, sizeof(expected), "@label=foo :%s BATCH +%s labeled-response" CRLF, me.name, batch5);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s ", batch5);
	while (*(line = get_client_sendq(user)) != '\0')
	{
		if (*line == '@')
		{
			*(line + strlen(expected)) = '\0';
			is_string(expected, line, MSG);
		}
		else
			break;
	}
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF, me.name, batch5);
	is_string(expected, line, MSG);
	is_client_sendq_empty(user, MSG);

	standard_free();
}

static void remote_response__hunted_server(void)
{
	char expected[BUFSIZE];
	rb_dictionary *dict = rb_dictionary_get_for_tests("remote labeled responses");

	standard_init();
	SetClientCap(user, CLICAP_LABELED_RESPONSE | CLICAP_BATCH);

	client_util_parse(user, "@label=foo TIME " TEST_SERVER_NAME);
	snprintf(expected, sizeof(expected), "@label=foo :%s BATCH +%s labeled-response" CRLF, me.name, batch1);
	is_client_sendq(expected, user, MSG);
	snprintf(expected, sizeof(expected),
		"@time=" ADVENTURE_TIME ";solanum.chat/response=%s,%s,%s :%s TIME :" TEST_SERVER_ID CRLF,
		user->id, batch1, server->name, user->id);
	is_client_sendq(expected, server, MSG);
	is_int(1, rb_dictionary_size(dict), MSG);
	is_int(1, rb_dlink_list_length(&user->localClient->pending_remote_responses), MSG);

	snprintf(expected, sizeof(expected),
		"@solanum.chat/response=%s,%s,%s :%s 391 %s %s :Friday July 14 2017 -- 02:40:00 +00:00" CRLF,
		user->id, batch1, server->name, server->id, user->id, server->name);
	client_util_parse(server, expected);
	snprintf(expected, sizeof(expected),
		"@batch=%s :%s 391 %s %s :Friday July 14 2017 -- 02:40:00 +00:00" CRLF,
		batch1, server->name, user->name, server->name);
	is_client_sendq(expected, user, MSG);
	is_int(1, rb_dictionary_size(dict), MSG);
	is_int(1, rb_dlink_list_length(&user->localClient->pending_remote_responses), MSG);

	snprintf(expected, sizeof(expected), "@solanum.chat/response=%s,%s,%s :%s ENCAP %s ACK", user->id, batch1, server->name, server->id, me.name);
	client_util_parse(server, expected);
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF, me.name, batch1);
	is_client_sendq(expected, user, MSG);
	is_int(0, rb_dictionary_size(dict), MSG);
	is_int(0, rb_dlink_list_length(&user->localClient->pending_remote_responses), MSG);

	standard_free();
}

static void remote_response__hunted_client(void)
{
	char expected[BUFSIZE];
	rb_dictionary *dict = rb_dictionary_get_for_tests("remote labeled responses");

	standard_init();
	SetClientCap(user, CLICAP_LABELED_RESPONSE | CLICAP_BATCH);

	client_util_parse(user, "@label=foo TIME " TEST_REMOTE_NICK);
	snprintf(expected, sizeof(expected), "@label=foo :%s BATCH +%s labeled-response" CRLF, me.name, batch1);
	is_client_sendq(expected, user, MSG);
	snprintf(expected, sizeof(expected),
		"@time=" ADVENTURE_TIME ";solanum.chat/response=%s,%s,%s :%s TIME :" TEST_REMOTE_ID CRLF,
		user->id, batch1, server->name, user->id);
	is_client_sendq(expected, server, MSG);
	is_int(1, rb_dictionary_size(dict), MSG);
	is_int(1, rb_dlink_list_length(&user->localClient->pending_remote_responses), MSG);
	is_int(1, ((struct ResponseInfo *)user->localClient->pending_remote_responses.head->data)->remote_response, MSG);

	snprintf(expected, sizeof(expected),
		"@solanum.chat/response=%s,%s,%s :%s 391 %s %s :Friday July 14 2017 -- 02:40:00 +00:00" CRLF,
		user->id, batch1, server->name, server->id, user->id, server->name);
	client_util_parse(server, expected);
	snprintf(expected, sizeof(expected),
		"@batch=%s :%s 391 %s %s :Friday July 14 2017 -- 02:40:00 +00:00" CRLF,
		batch1, server->name, user->name, server->name);
	is_client_sendq(expected, user, MSG);
	is_int(1, rb_dictionary_size(dict), MSG);
	is_int(1, rb_dlink_list_length(&user->localClient->pending_remote_responses), MSG);

	snprintf(expected, sizeof(expected), "@solanum.chat/response=%s,%s,%s :%s ENCAP %s ACK", user->id, batch1, server->name, server->id, me.name);
	client_util_parse(server, expected);
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF, me.name, batch1);
	is_client_sendq(expected, user, MSG);
	is_int(0, rb_dictionary_size(dict), MSG);
	is_int(0, rb_dlink_list_length(&user->localClient->pending_remote_responses), MSG);

	standard_free();
}

static void remote_response__mask(void)
{
	char expected[BUFSIZE];
	rb_dictionary *dict = rb_dictionary_get_for_tests("remote labeled responses");

	standard_init();
	SetClientCap(user, CLICAP_LABELED_RESPONSE | CLICAP_BATCH);
	make_local_person_admin(user);

	client_util_parse(user, "@label=foo MODLIST foo remote*");
	snprintf(expected, sizeof(expected), "@label=foo :%s BATCH +%s labeled-response" CRLF, me.name, batch1);
	is_client_sendq(expected, user, MSG);
	snprintf(expected, sizeof(expected),
		"@time=" ADVENTURE_TIME ";solanum.chat/response=%s,%s,remote* :%s ENCAP remote* MODLIST foo" CRLF,
		user->id, batch1, user->id);
	is_client_sendq(expected, server, MSG);
	is_int(1, rb_dictionary_size(dict), MSG);
	is_int(1, rb_dlink_list_length(&user->localClient->pending_remote_responses), MSG);
	is_int(3, ((struct ResponseInfo *)user->localClient->pending_remote_responses.head->data)->remote_response, MSG);

	snprintf(expected, sizeof(expected), "@solanum.chat/response=%s,%s,remote* :%s ENCAP %s ACK", user->id, batch1, server->id, me.name);
	client_util_parse(server, expected);
	snprintf(expected, sizeof(expected), "@solanum.chat/response=%s,%s,remote* :%s ENCAP %s ACK", user->id, batch1, server2->id, me.name);
	client_util_parse(server2, expected);
	snprintf(expected, sizeof(expected), "@solanum.chat/response=%s,%s,remote* :%s ENCAP %s ACK", user->id, batch1, server3->id, me.name);
	client_util_parse(server3, expected);

	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF, me.name, batch1);
	is_client_sendq(expected, user, MSG);
	is_int(0, rb_dictionary_size(dict), MSG);
	is_int(0, rb_dlink_list_length(&user->localClient->pending_remote_responses), MSG);

	standard_free();
}

static void remote_response__timeout(void)
{
	char expected[BUFSIZE];
	rb_dictionary *dict = rb_dictionary_get_for_tests("remote labeled responses");

	standard_init();
	SetClientCap(user, CLICAP_LABELED_RESPONSE | CLICAP_BATCH);

	client_util_parse(user, "@label=foo WHOIS " TEST_REMOTE_NICK " " TEST_REMOTE_NICK);
	snprintf(expected, sizeof(expected), "@label=foo :%s BATCH +%s labeled-response" CRLF, me.name, batch1);
	is_client_sendq(expected, user, MSG);
	is_int(1, rb_dictionary_size(dict), MSG);
	is_int(1, rb_dlink_list_length(&user->localClient->pending_remote_responses), MSG);

	/* backdate the expiration so the event cleans it up */
	struct ResponseInfo *info = user->localClient->pending_remote_responses.head->data;
	info->expires = 1;
	rb_run_one_event_for_tests("cleanup_pending_responses");
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF, me.name, batch1);
	is_client_sendq(expected, user, MSG);
	is_int(0, rb_dictionary_size(dict), MSG);
	is_int(0, rb_dlink_list_length(&user->localClient->pending_remote_responses), MSG);

	/* late response doesn't carry batch tag and late ACK does nothing */
	snprintf(expected, sizeof(expected),
		"@solanum.chat/response=%s,%s,%s :%s 318 %s %s :End of /WHOIS list.",
		user->id, batch1, server->name, server->id, user->name, remote->name);
	client_util_parse(server, expected);
	snprintf(expected, sizeof(expected), ":%s 318 %s %s :End of /WHOIS list." CRLF, server->name, user->name, remote->name);
	is_client_sendq(expected, user, MSG);
	snprintf(expected, sizeof(expected),
		"@solanum.chat/response=%s,%s,%s :%s ENCAP %s ACK",
		user->id, batch1, server->name, server->id, me.name);
	client_util_parse(server, expected);
	is_client_sendq_empty(user, MSG);

	standard_free();
}

static void response_tag(void)
{
	char expected[BUFSIZE];

	standard_init();

	/* if we match the mask (3rd part of the tag), we need to send back ENCAP ACK */
	client_util_parse(server, "@solanum.chat/response=" TEST_REMOTE_ID ",foo,* :" TEST_REMOTE_ID " TIME :" TEST_ME_ID);
	snprintf(expected, sizeof(expected),
		"@time=" ADVENTURE_TIME ";solanum.chat/response=%s,foo,* :%s 391 %s %s :Friday July 14 2017 -- 02:40:00 +00:00" CRLF,
		remote->id, me.id, remote->id, me.name);
	is_client_sendq_one(expected, server, MSG);
	snprintf(expected, sizeof(expected),
		"@time=" ADVENTURE_TIME ";solanum.chat/response=%s,foo,* :%s ENCAP %s ACK" CRLF,
		remote->id, me.id, server->name);
	is_client_sendq(expected, server, MSG);

	/* no ACK if we don't match the mask */
	client_util_parse(server, "@solanum.chat/response=" TEST_REMOTE_ID ",foo,badmask :" TEST_REMOTE_ID " TIME :" TEST_ME_ID);
	snprintf(expected, sizeof(expected),
		"@time=" ADVENTURE_TIME ";solanum.chat/response=%s,foo,badmask :%s 391 %s %s :Friday July 14 2017 -- 02:40:00 +00:00" CRLF,
		remote->id, me.id, remote->id, me.name);
	is_client_sendq(expected, server, MSG);

	/* invalid tags don't get passed (and also have no ACK) */
	client_util_parse(server, "@solanum.chat/response=invalid,value :" TEST_REMOTE_ID " TIME :" TEST_ME_ID);
	snprintf(expected, sizeof(expected),
		"@time=" ADVENTURE_TIME " :%s 391 %s %s :Friday July 14 2017 -- 02:40:00 +00:00" CRLF,
		me.id, remote->id, me.name);
	is_client_sendq(expected, server, MSG);

	/* we still do an ACK if we get an ENCAP not destined for us */
	client_util_parse(server, "@solanum.chat/response=" TEST_REMOTE_ID ",foo,* :" TEST_REMOTE_ID " ENCAP badmask FOO");
	snprintf(expected, sizeof(expected),
		"@time=" ADVENTURE_TIME ";solanum.chat/response=%s,foo,* :%s ENCAP %s ACK" CRLF,
		remote->id, me.id, server->name);
	is_client_sendq(expected, server, MSG);

	standard_free();
}

static void response_cleanup(void)
{
	char expected[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_LABELED_RESPONSE | CLICAP_BATCH);

	client_util_parse(user, "@label=foo PRIVMSG " TEST_REMOTE_NICK " :hi");
	snprintf(expected, sizeof(expected), "@label=foo :%s BATCH +%s labeled-response" CRLF, me.name, batch1);
	is_client_sendq(expected, user, MSG);
	/* ensure outgoing_response_info is NULL after parsing a command;
	 * otherwise, future commands will inherit incorrect context (and possibly crash) */
	is_hex(0, (uintptr_t)outgoing_response_info, "outgoing_response_info not cleaned up: " MSG);

	standard_free();
}

static void safelist_response(void)
{
	char expected[BUFSIZE];
	int refills = 0;

	standard_init();
	init_large_list();

	client_util_parse(user, "@label=foo LIST #safelist*,>0");
	snprintf(expected, sizeof(expected), "@label=foo :%s BATCH +%s labeled-response" CRLF, me.name, batch1);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s 321 %s Channel :Users  Name" CRLF, batch1, me.name, user->name);
	is_client_sendq_one(expected, user, MSG);

	for (int i = 0; i < 100; i++)
	{
		snprintf(expected, sizeof(expected), "@batch=%s :%s 322 %s #safelist%02d 1 :" LONG_VALUE_200 CRLF, batch1, me.name, user->name, i);
		is_client_sendq_one(expected, user, MSG);

		if (rb_linebuf_len(&user->localClient->buf_sendq) == 0 && user->localClient->safelist_data != NULL)
		{
			rb_run_one_event_for_tests("safelist_iterate_clients");
			if (rb_linebuf_len(&user->localClient->buf_sendq) == 0)
			{
				ok(0, "safelist_iterate_clients failed; " MSG);
				return;
			}

			refills++;
		}
	}

	snprintf(expected, sizeof(expected), "@batch=%s :%s 323 %s :End of /LIST" CRLF, batch1, me.name, user->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF, me.name, batch1);
	is_client_sendq(expected, user, MSG);
	ok(user->localClient->safelist_data == NULL, MSG);
	ok(refills > 0, MSG);

	standard_free();
}

static void safelist_response__exit(void)
{
	char expected[BUFSIZE];

	standard_init();
	init_large_list();

	client_util_parse(user, "@label=foo LIST #safelist*,>0");
	snprintf(expected, sizeof(expected), "@label=foo :%s BATCH +%s labeled-response" CRLF, me.name, batch1);
	is_client_sendq_one(expected, user, MSG);

	/* empty the sendq to make room for more messages.
	 * we're already testing the contents in a different test, so for this one just clear it */
	drain_client_sendq(user);

	/* verify the user still has LIST in progress */
	ok(user->localClient->safelist_data != NULL, MSG);

	/* this should trip ASAN (and probably cause a crash even on non-ASAN builds)
	 * if the client wasn't fully cleaned up */
	remove_local_person(user);
	rb_run_one_event_for_tests("free_exited_clients");
	rb_run_one_event_for_tests("safelist_iterate_clients");

	user = NULL;
	standard_free();
}

static void safelist_response__part(void)
{
	char expected[BUFSIZE];

	standard_init();
	init_large_list();

	client_util_parse(user, "@label=foo LIST #safelist*,>0");
	snprintf(expected, sizeof(expected), "@label=foo :%s BATCH +%s labeled-response" CRLF, me.name, batch1);
	is_client_sendq_one(expected, user, MSG);

	/* empty the sendq to make room for more messages.
	 * we're already testing the contents in a different test, so for this one just clear it */
	drain_client_sendq(user);

	/* verify the user still has LIST in progress */
	ok(user->localClient->safelist_data != NULL, MSG);

	/* this will destroy every #safelist* channel since user is the only member,
	 * meaning there should be no more channels left to iterate after next event execution */
	remove_user_from_channels(user);
	rb_run_one_event_for_tests("safelist_iterate_clients");
	snprintf(expected, sizeof(expected), "@batch=%s :%s 323 %s :End of /LIST" CRLF, batch1, me.name, user->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF, me.name, batch1);
	is_client_sendq(expected, user, MSG);

	is_hex(0, (uintptr_t)user->localClient->safelist_data, MSG);

	standard_free();
}

static void safelist_response__abort(void)
{
	char expected[BUFSIZE];

	standard_init();
	init_large_list();

	client_util_parse(user, "@label=foo LIST #safelist*,>0");
	snprintf(expected, sizeof(expected), "@label=foo :%s BATCH +%s labeled-response" CRLF, me.name, batch1);
	is_client_sendq_one(expected, user, MSG);

	/* empty the sendq to make room for more messages.
	 * we're already testing the contents in a different test, so for this one just clear it */
	drain_client_sendq(user);

	/* verify the user still has LIST in progress */
	ok(user->localClient->safelist_data != NULL, MSG);

	/* sending another list aborts the first and returns no response for the second */
	client_util_parse(user, "@label=bar LIST >0");
	snprintf(expected, sizeof(expected), "@batch=%s :%s NOTICE %s :/LIST aborted" CRLF, batch1, me.name, user->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s 323 %s :End of /LIST" CRLF, batch1, me.name, user->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF, me.name, batch1);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@label=bar :%s ACK" CRLF, me.name);
	is_client_sendq(expected, user, MSG);

	is_hex(0, (uintptr_t)user->localClient->safelist_data, MSG);

	standard_free();
}

int main(int argc, char *argv[])
{
	/* we call TIME which uses localtime(), so ensure we're in UTC */
	rb_setenv("TZ", "UTC", 1);
	tzset();

	plan_lazy();

	ircd_util_init(__FILE__);
	client_util_init();

	srand(0);
	generate_batch_id(batch1, sizeof(batch1));
	generate_batch_id(batch2, sizeof(batch2));
	generate_batch_id(batch3, sizeof(batch3));
	generate_batch_id(batch4, sizeof(batch4));
	generate_batch_id(batch5, sizeof(batch5));
	generate_batch_id(batch6, sizeof(batch6));
	generate_batch_id(batch7, sizeof(batch7));
	generate_batch_id(batch8, sizeof(batch8));

	single_response();
	single_response__multi_client();
	multi_response();
	remote_response__hunted_server();
	remote_response__hunted_client();
	remote_response__mask();
	remote_response__timeout();

	response_tag();
	response_cleanup();

	safelist_response();
	safelist_response__exit();
	safelist_response__part();
	safelist_response__abort();

	client_util_free();
	ircd_util_free();
	return 0;
}
