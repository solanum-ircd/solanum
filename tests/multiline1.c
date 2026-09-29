/*
 * multiline1.c: Test the draft/multiline batch type
 *
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA
 */

#include "stdinc.h"
#include "tap/basic.h"

#include "batch.h"
#include "capability.h"
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "ircd_util.h"
#include "client_util.h"
#include "modules.h"
#include "s_serv.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__

static uint64_t CLICAP_MULTILINE;
static uint64_t CAP_MULTILINE;
static uint64_t CAP_ECHOB;

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

static void
local_private(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);
	SetClientCap(local_chan_p, CLICAP_BATCH | CLICAP_MULTILINE);

	snprintf(line, sizeof(line), "BATCH +private draft/multiline %s" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=private PRIVMSG %s :one" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=private PRIVMSG %s :" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	client_util_parse(user, "BATCH -private" CRLF);

	snprintf(expected, sizeof(expected), ":%s!%s@%s BATCH +%s draft/multiline %s" CRLF,
		user->name, user->username, user->host, batch1, local_chan_p->name);
	is_client_sendq_one(expected, local_chan_p, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s!%s@%s PRIVMSG %s :one" CRLF,
		batch1, user->name, user->username, user->host, local_chan_p->name);
	is_client_sendq_one(expected, local_chan_p, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s!%s@%s PRIVMSG %s :" CRLF,
		batch1, user->name, user->username, user->host, local_chan_p->name);
	is_client_sendq_one(expected, local_chan_p, MSG);
	snprintf(expected, sizeof(expected), ":%s!%s@%s BATCH -%s" CRLF,
		user->name, user->username, user->host, batch1);
	is_client_sendq(expected, local_chan_p, MSG);

	standard_free();
}

static void
local_private__fallback(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);

	snprintf(line, sizeof(line), "BATCH +private draft/multiline %s" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=private PRIVMSG %s :one" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=private PRIVMSG %s :" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	client_util_parse(user, "BATCH -private" CRLF);

	snprintf(expected, sizeof(expected), ":%s!%s@%s PRIVMSG %s :one" CRLF,
		user->name, user->username, user->host, local_chan_p->name);
	is_client_sendq(expected, local_chan_p, MSG);

	standard_free();
}

static void
local_private__fallback__batch_only(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);
	SetClientCap(local_chan_p, CLICAP_BATCH);

	snprintf(line, sizeof(line), "BATCH +private draft/multiline %s" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=private PRIVMSG %s :one" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=private PRIVMSG %s :" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	client_util_parse(user, "BATCH -private" CRLF);

	snprintf(expected, sizeof(expected), ":%s!%s@%s PRIVMSG %s :one" CRLF,
		user->name, user->username, user->host, local_chan_p->name);
	is_client_sendq(expected, local_chan_p, MSG);

	standard_free();
}

static void
local_private__fallback__multiline_only(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);
	SetClientCap(local_chan_p, CLICAP_MULTILINE);

	snprintf(line, sizeof(line), "BATCH +private draft/multiline %s" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=private PRIVMSG %s :one" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=private PRIVMSG %s :" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	client_util_parse(user, "BATCH -private" CRLF);

	snprintf(expected, sizeof(expected), ":%s!%s@%s PRIVMSG %s :one" CRLF,
		user->name, user->username, user->host, local_chan_p->name);
	is_client_sendq(expected, local_chan_p, MSG);

	standard_free();
}

static void
local_channel(void)
{
	char expected[BUFSIZE];

	standard_init();
	SetClientCap(local_chan_o, CLICAP_BATCH | CLICAP_MULTILINE);
	SetClientCap(local_chan_p, CLICAP_BATCH | CLICAP_MULTILINE);

	client_util_parse(local_chan_o, "BATCH +channel draft/multiline " TEST_CHANNEL CRLF);
	client_util_parse(local_chan_o, "@batch=channel PRIVMSG " TEST_CHANNEL " :one" CRLF);
	client_util_parse(local_chan_o, "@batch=channel;draft/multiline-concat PRIVMSG " TEST_CHANNEL " :two" CRLF);
	client_util_parse(local_chan_o, "BATCH -channel" CRLF);

	snprintf(expected, sizeof(expected), ":%s!%s@%s BATCH +%s draft/multiline %s" CRLF,
		local_chan_o->name, local_chan_o->username, local_chan_o->host, batch1, TEST_CHANNEL);
	is_client_sendq_one(expected, local_chan_p, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s!%s@%s PRIVMSG %s :one" CRLF,
		batch1, local_chan_o->name, local_chan_o->username, local_chan_o->host, TEST_CHANNEL);
	is_client_sendq_one(expected, local_chan_p, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s;draft/multiline-concat :%s!%s@%s PRIVMSG %s :two" CRLF,
		batch1, local_chan_o->name, local_chan_o->username, local_chan_o->host, TEST_CHANNEL);
	is_client_sendq_one(expected, local_chan_p, MSG);
	snprintf(expected, sizeof(expected), ":%s!%s@%s BATCH -%s" CRLF,
		local_chan_o->name, local_chan_o->username, local_chan_o->host, batch1);
	is_client_sendq(expected, local_chan_p, MSG);

	standard_free();
}

static void
local_channel__denied(void)
{
	char line[BUFSIZE];

	standard_init();
	add_user_to_channel(channel, user, CHFL_PEON);
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);
	SetClientCap(local_chan_o, CLICAP_BATCH | CLICAP_MULTILINE);
	channel->mode.mode |= MODE_MODERATED;

	client_util_parse(user, "BATCH +denied draft/multiline " TEST_CHANNEL CRLF);
	snprintf(line, sizeof(line), "@batch=denied PRIVMSG %s :one" CRLF,
		channel->chname);
	client_util_parse(user, line);
	client_util_parse(user, "BATCH -denied" CRLF);

	is_client_sendq(":" TEST_ME_NAME " 404 " TEST_NICK " " TEST_CHANNEL " :Cannot send to nick/channel" CRLF, user, MSG);
	is_client_sendq_empty(local_chan_o, MSG);

	standard_free();
}

static void
local_validation__mixed_commands(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);
	SetClientCap(local_chan_p, CLICAP_BATCH | CLICAP_MULTILINE);

	snprintf(line, sizeof(line), "BATCH +mixed draft/multiline %s" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=mixed PRIVMSG %s :first" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=mixed NOTICE %s :second" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	client_util_parse(user, "BATCH -mixed" CRLF);
	snprintf(expected, sizeof(expected), ":%s FAIL BATCH MULTILINE_INVALID :multiline batch must only have either PRIVMSG or NOTICE commands" CRLF,
		me.name);
	is_client_sendq(expected, user, MSG);
	is_client_sendq_empty(local_chan_p, MSG);

	standard_free();
}

static void
local_validation__mismatched_target(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);

	snprintf(line, sizeof(line), "BATCH +target draft/multiline %s" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=target PRIVMSG other :first" CRLF);
	client_util_parse(user, line);
	client_util_parse(user, "BATCH -target" CRLF);
	snprintf(expected, sizeof(expected), ":%s FAIL BATCH MULTILINE_INVALID_TARGET %s other :mismatched target within multiline batch" CRLF,
		me.name, local_chan_p->name);
	is_client_sendq(expected, user, MSG);
	is_client_sendq_empty(local_chan_p, MSG);

	standard_free();
}

static void
local_validation__blank_concat(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);

	snprintf(line, sizeof(line), "BATCH +blank draft/multiline %s" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=blank PRIVMSG %s :first" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=blank;draft/multiline-concat PRIVMSG %s :" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	client_util_parse(user, "BATCH -blank" CRLF);
	snprintf(expected, sizeof(expected), ":%s FAIL BATCH MULTILINE_INVALID :cannot send blank line with draft/multiline-concat" CRLF,
		me.name);
	is_client_sendq(expected, user, MSG);
	is_client_sendq_empty(local_chan_p, MSG);

	standard_free();
}

static void
local_validation__empty_batch(void)
{
	char expected[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);

	client_util_parse(user, "BATCH +empty draft/multiline " TEST_NICK CRLF);
	client_util_parse(user, "BATCH -empty" CRLF);

	snprintf(expected, sizeof(expected), ":%s FAIL BATCH MULTILINE_INVALID :multiline batches cannot be empty" CRLF,
		me.name);
	is_client_sendq(expected, user, MSG);

	standard_free();
}

static void
local_validation__blank_lines(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);

	snprintf(line, sizeof(line), "BATCH +blanklines draft/multiline %s" CRLF, local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=blanklines PRIVMSG %s :" CRLF, local_chan_p->name);
	client_util_parse(user, line);
	client_util_parse(user, line);
	client_util_parse(user, line);
	client_util_parse(user, "BATCH -blanklines" CRLF);

	snprintf(expected, sizeof(expected), ":%s FAIL BATCH MULTILINE_INVALID :multiline batch cannot consist solely of blank lines" CRLF,
		me.name);
	is_client_sendq(expected, user, MSG);
	is_client_sendq_empty(local_chan_p, MSG);

	standard_free();
}

static void
local_validation__missing_target(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);

	client_util_parse(user, "BATCH +missing draft/multiline" CRLF);
	snprintf(line, sizeof(line), "@batch=missing PRIVMSG %s :hi" CRLF,
		local_chan_p->name);
	client_util_parse(user, line);
	client_util_parse(user, "BATCH -missing" CRLF);

	snprintf(expected, sizeof(expected), ":%s FAIL BATCH MULTILINE_INVALID :multiline batch is missing a target" CRLF,
		me.name);
	is_client_sendq(expected, user, MSG);

	standard_free();
}

static void
local_validation__max_lines(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);

	snprintf(line, sizeof(line), "BATCH +lines draft/multiline %s" CRLF, local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=lines PRIVMSG %s :one" CRLF, local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=lines PRIVMSG %s :two" CRLF, local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=lines PRIVMSG %s :three" CRLF, local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=lines PRIVMSG %s :four" CRLF, local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=lines PRIVMSG %s :five" CRLF, local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=lines PRIVMSG %s :six" CRLF, local_chan_p->name);
	client_util_parse(user, line);
	client_util_parse(user, "BATCH -lines" CRLF);
	snprintf(expected, sizeof(expected), ":%s FAIL BATCH MULTILINE_MAX_LINES 5 :multiline batch contains too many lines" CRLF, me.name);
	is_client_sendq(expected, user, MSG);
	is_client_sendq_empty(local_chan_p, MSG);

	standard_free();
}

static void
local_validation__max_bytes(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);

	snprintf(line, sizeof(line), "BATCH +bytes draft/multiline %s" CRLF, local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=bytes PRIVMSG %s :1234567890123456789012345" CRLF, local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=bytes PRIVMSG %s :12345678901234567890123456" CRLF, local_chan_p->name);
	client_util_parse(user, line);
	client_util_parse(user, "BATCH -bytes" CRLF);
	snprintf(expected, sizeof(expected), ":%s FAIL BATCH MULTILINE_MAX_BYTES 50 :multiline batch contains too many bytes" CRLF, me.name);
	is_client_sendq(expected, user, MSG);
	is_client_sendq_empty(local_chan_p, MSG);

	standard_free();
}

static void
local_validation__non_message_command(void)
{
	char expected[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);

	client_util_parse(user, "BATCH +command draft/multiline " TEST_CHANNEL CRLF);
	client_util_parse(user, "@batch=command JOIN " TEST_CHANNEL CRLF);
	client_util_parse(user, "BATCH -command" CRLF);

	snprintf(expected, sizeof(expected), ":%s FAIL BATCH MULTILINE_INVALID :multiline batch must only have either PRIVMSG or NOTICE commands" CRLF, me.name);
	is_client_sendq(expected, user, MSG);

	standard_free();
}

static void
local_validation__nested(void)
{
	char expected[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);

	client_util_parse(user, "BATCH +outer draft/multiline " TEST_CHANNEL CRLF);
	client_util_parse(user, "@batch=outer BATCH +inner draft/multiline " TEST_CHANNEL CRLF);

	snprintf(expected, sizeof(expected), ":%s FAIL BATCH INVALID_NESTING +inner draft/multiline draft/multiline :The parent batch type does not allow this type to be nested under it" CRLF, me.name);
	is_client_sendq(expected, user, MSG);

	standard_free();
}

static void
local_validation__timeout(void)
{
	struct Batch *batch;
	char expected[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);

	client_util_parse(user, "BATCH +timeout draft/multiline " TEST_CHANNEL CRLF);
	batch = user->localClient->pending_batches.head->data;
	batch->expires = 1;
	rb_run_one_event_for_tests("batch-timeout");

	snprintf(expected, sizeof(expected), ":%s FAIL BATCH TIMEOUT timeout :Batch timed out" CRLF, me.name);
	is_client_sendq(expected, user, MSG);
	standard_free();
}

static void
local_validation__missing_caps(void)
{
	char expected[BUFSIZE];

	standard_init();

	client_util_parse(user, "BATCH +missing draft/multiline " TEST_NICK CRLF);
	client_util_parse(user, "@batch=missing PRIVMSG " TEST_NICK " :one" CRLF);
	client_util_parse(user, "BATCH -missing" CRLF);

	snprintf(expected, sizeof(expected), ":%s FAIL BATCH MULTILINE_INVALID :multiline batches require both the batch and draft/multiline capabilities" CRLF, me.name);
	is_client_sendq(expected, user, MSG);

	standard_free();
}

static void
local_validation__s2s_echo_batch(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();

	snprintf(line, sizeof(line), "BATCH +echo solanum.chat/echo %s" CRLF, local_chan_p->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=echo PRIVMSG %s :spoofed" CRLF, local_chan_p->name);
	client_util_parse(user, line);
	client_util_parse(user, "BATCH -echo" CRLF);

	snprintf(expected, sizeof(expected), ":%s FAIL BATCH UNKNOWN_TYPE echo solanum.chat/echo :Unrecognized batch type" CRLF,
		me.name);
	is_client_sendq(expected, user, MSG);
	is_client_sendq_empty(local_chan_p, MSG);

	standard_free();
}

static void
local_validation__invalid_reference_tag(void)
{
	char expected[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);

	client_util_parse(user, "BATCH +bad/tag draft/multiline " TEST_NICK CRLF);

	snprintf(expected, sizeof(expected), ":%s FAIL BATCH INVALID_REFTAG +bad/tag :Invalid reference tag" CRLF,
		me.name);
	is_client_sendq(expected, user, MSG);
	is_int(0, rb_dlink_list_length(&user->localClient->pending_batches), MSG);

	standard_free();
}

static void
remote_to_local(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetServerCap(server, CAP_MULTILINE);
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);

	snprintf(line, sizeof(line), ":%s BATCH +remote draft/multiline %s" CRLF, remote->id, user->id);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=remote :%s PRIVMSG %s :first" CRLF, remote->id, user->id);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=remote;draft/multiline-concat :%s PRIVMSG %s :second" CRLF, remote->id, user->id);
	client_util_parse(server, line);
	client_util_parse(server, ":" TEST_REMOTE_ID " BATCH -remote" CRLF);

	snprintf(expected, sizeof(expected), ":%s!%s@%s BATCH +%s draft/multiline %s" CRLF,
		remote->name, remote->username, remote->host, batch1, user->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s!%s@%s PRIVMSG %s :first" CRLF,
		batch1, remote->name, remote->username, remote->host, user->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s;draft/multiline-concat :%s!%s@%s PRIVMSG %s :second" CRLF,
		batch1, remote->name, remote->username, remote->host, user->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), ":%s!%s@%s BATCH -%s" CRLF,
		remote->name, remote->username, remote->host, batch1);
	is_client_sendq(expected, user, MSG);

	standard_free();
}

static void
remote_to_local__echo_batch(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetServerCap(server, CAP_MULTILINE | CAP_ECHOB);
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE);

	snprintf(line, sizeof(line), ":%s BATCH +remote draft/multiline %s" CRLF, remote->id, user->id);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=remote :%s PRIVMSG %s :first" CRLF, remote->id, user->id);
	client_util_parse(server, line);
	client_util_parse(server, ":" TEST_REMOTE_ID " BATCH -remote" CRLF);

	drain_client_sendq(user);
	snprintf(expected, sizeof(expected), ":%s BATCH +%s solanum.chat/echo %s" CRLF,
		user->id, batch2, remote->id);
	is_client_sendq_one(expected, server, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s BATCH +remote draft/multiline %s" CRLF,
		batch2, user->id, user->name);
	is_client_sendq_one(expected, server, MSG);
	snprintf(expected, sizeof(expected), "@batch=remote :%s PRIVMSG %s :first" CRLF,
		user->id, user->name);
	is_client_sendq_one(expected, server, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s BATCH -remote" CRLF,
		batch2, user->id);
	is_client_sendq_one(expected, server, MSG);
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF, user->id, batch2);
	is_client_sendq(expected, server, MSG);

	standard_free();
}

static void
remote_to_remote(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetServerCap(server, CAP_MULTILINE);
	SetServerCap(server2, CAP_MULTILINE);

	snprintf(line, sizeof(line), ":%s BATCH +remote draft/multiline %s" CRLF, remote->id, remote2->id);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=remote :%s PRIVMSG %s :first" CRLF, remote->id, remote2->id);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=remote;draft/multiline-concat :%s PRIVMSG %s :second" CRLF, remote->id, remote2->id);
	client_util_parse(server, line);
	client_util_parse(server, ":" TEST_REMOTE_ID " BATCH -remote" CRLF);

	snprintf(expected, sizeof(expected), ":%s!%s@%s BATCH +%s draft/multiline %s" CRLF,
		remote->id, remote->username, remote->host, batch1, remote2->name);
	is_client_sendq_one(expected, server2, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s!%s@%s PRIVMSG %s :first" CRLF,
		batch1, remote->id, remote->username, remote->host, remote2->id);
	is_client_sendq_one(expected, server2, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s;draft/multiline-concat :%s!%s@%s PRIVMSG %s :second" CRLF,
		batch1, remote->id, remote->username, remote->host, remote2->id);
	is_client_sendq_one(expected, server2, MSG);
	snprintf(expected, sizeof(expected), ":%s!%s@%s BATCH -%s" CRLF,
		remote->id, remote->username, remote->host, batch1);
	is_client_sendq(expected, server2, MSG);

	standard_free();
}

static void
remote_to_remote__fallback(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetServerCap(server, CAP_MULTILINE);

	snprintf(line, sizeof(line), ":%s BATCH +remote draft/multiline %s" CRLF, remote->id, remote2->id);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=remote :%s PRIVMSG %s :first" CRLF, remote->id, remote2->id);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=remote;draft/multiline-concat :%s PRIVMSG %s :second" CRLF, remote->id, remote2->id);
	client_util_parse(server, line);
	client_util_parse(server, ":" TEST_REMOTE_ID " BATCH -remote" CRLF);

	snprintf(expected, sizeof(expected), ":%s!%s@%s PRIVMSG %s :first" CRLF,
		remote->id, remote->username, remote->host, remote2->id);
	is_client_sendq_one(expected, server2, MSG);
	snprintf(expected, sizeof(expected), ":%s!%s@%s PRIVMSG %s :second" CRLF,
		remote->id, remote->username, remote->host, remote2->id);
	is_client_sendq(expected, server2, MSG);

	standard_free();
}

static void
remote_to_opmod_channel(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetServerCap(server, CAP_MULTILINE);
	SetServerCap(server2, CAP_MULTILINE | CAP_CHW | CAP_EOPMOD);
	channel->mode.mode |= MODE_OPMODERATE;

	snprintf(line, sizeof(line), ":%s BATCH +opmod draft/multiline =%s" CRLF, remote_chan_p->id, TEST_CHANNEL);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=opmod :%s PRIVMSG =%s :one" CRLF, remote_chan_p->id, TEST_CHANNEL);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=opmod;draft/multiline-concat :%s PRIVMSG =%s :two" CRLF, remote_chan_p->id, TEST_CHANNEL);
	client_util_parse(server, line);
	client_util_parse(server, ":" TEST_SERVER_ID "90104 BATCH -opmod" CRLF);

	snprintf(expected, sizeof(expected), ":%s BATCH +%s draft/multiline =%s" CRLF, remote_chan_p->id, batch1, TEST_CHANNEL);
	is_client_sendq_one(expected, server2, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s PRIVMSG =%s :one" CRLF, batch1, remote_chan_p->id, TEST_CHANNEL);
	is_client_sendq_one(expected, server2, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s;draft/multiline-concat :%s PRIVMSG =%s :two" CRLF, batch1, remote_chan_p->id, TEST_CHANNEL);
	is_client_sendq_one(expected, server2, MSG);
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF, remote_chan_p->id, batch1);
	is_client_sendq(expected, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	standard_free();
}

static void
remote_to_opmod_channel__no_eopmod(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetServerCap(server, CAP_MULTILINE);
	SetServerCap(server2, CAP_MULTILINE | CAP_CHW);
	channel->mode.mode |= MODE_OPMODERATE;

	snprintf(line, sizeof(line), ":%s BATCH +opmod draft/multiline =%s" CRLF, remote_chan_p->id, TEST_CHANNEL);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=opmod :%s PRIVMSG =%s :one" CRLF, remote_chan_p->id, TEST_CHANNEL);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=opmod;draft/multiline-concat :%s PRIVMSG =%s :two" CRLF, remote_chan_p->id, TEST_CHANNEL);
	client_util_parse(server, line);
	client_util_parse(server, ":" TEST_SERVER_ID "90104 BATCH -opmod" CRLF);

	snprintf(expected, sizeof(expected), ":%s BATCH +%s draft/multiline @%s" CRLF, server->id, batch1, TEST_CHANNEL);
	is_client_sendq_one(expected, server2, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s NOTICE @%s :<%s:%s> one" CRLF,
		batch1, server->id, TEST_CHANNEL, remote_chan_p->name, TEST_CHANNEL);
	is_client_sendq_one(expected, server2, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s;draft/multiline-concat :%s NOTICE @%s :<%s:%s> two" CRLF,
		batch1, server->id, TEST_CHANNEL, remote_chan_p->name, TEST_CHANNEL);
	is_client_sendq_one(expected, server2, MSG);
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF, remote_chan_p->id, batch1);
	is_client_sendq(expected, server2, MSG);
	is_client_sendq_empty(server3, MSG);

	standard_free();
}

static void
remote_to_opmod_channel__no_chw(void)
{
	char line[BUFSIZE];

	standard_init();
	SetServerCap(server, CAP_MULTILINE);
	SetServerCap(server2, CAP_MULTILINE | CAP_EOPMOD);
	channel->mode.mode |= MODE_OPMODERATE;

	snprintf(line, sizeof(line), ":%s BATCH +opmod draft/multiline =%s" CRLF, remote_chan_p->id, TEST_CHANNEL);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=opmod :%s PRIVMSG =%s :one" CRLF, remote_chan_p->id, TEST_CHANNEL);
	client_util_parse(server, line);
	client_util_parse(server, ":" TEST_SERVER_ID "90104 BATCH -opmod" CRLF);

	is_client_sendq_empty(server2, MSG);
	is_client_sendq_empty(server3, MSG);

	standard_free();
}

int
main(int argc, char *argv[])
{
	plan_lazy();

	ircd_util_init(__FILE__);
	client_util_init();

	/* so we don't need to check the @time tag on S2S links due to STAG being enabled */
	ok(unload_one_module("cap_server_time", false), MSG);

	CLICAP_MULTILINE = capability_get(cli_capindex, "draft/multiline", NULL);
	CAP_MULTILINE = capability_get(serv_capindex, "MULTILN", NULL);
	CAP_ECHOB = capability_get(serv_capindex, "ECHOB", NULL);
	ok(CLICAP_MULTILINE != 0, "draft/multiline client capability is loaded");
	ok(CAP_MULTILINE != 0, "MULTILN server capability is loaded");
	ok(CAP_ECHOB != 0, "ECHOB server capability is loaded");

	srand(0);
	generate_batch_id(batch1, sizeof(batch1));
	generate_batch_id(batch2, sizeof(batch2));
	generate_batch_id(batch3, sizeof(batch3));
	generate_batch_id(batch4, sizeof(batch4));
	generate_batch_id(batch5, sizeof(batch5));

	local_private();
	local_private__fallback();
	local_private__fallback__batch_only();
	local_private__fallback__multiline_only();
	local_channel();
	local_channel__denied();
	local_validation__mixed_commands();
	local_validation__mismatched_target();
	local_validation__blank_concat();
	local_validation__empty_batch();
	local_validation__blank_lines();
	local_validation__missing_target();
	local_validation__max_lines();
	local_validation__max_bytes();
	local_validation__non_message_command();
	local_validation__nested();
	local_validation__timeout();
	local_validation__missing_caps();
	local_validation__s2s_echo_batch();
	local_validation__invalid_reference_tag();
	remote_to_local();
	remote_to_local__echo_batch();
	remote_to_remote();
	remote_to_remote__fallback();
	remote_to_opmod_channel();
	remote_to_opmod_channel__no_eopmod();
	remote_to_opmod_channel__no_chw();

	client_util_free();
	ircd_util_free();
	return 0;
}
