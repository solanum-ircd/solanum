/*
 * multiline2.c: Test the draft/multiline batch type
 * These tests have remote labeled-response functionality disabled
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
#define MSGID "11500000000000000001" TEST_ID

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

static void (*reset_msgid_for_tests)(void); /* defined at the end of tag_message_id.c */

static uint64_t CLICAP_MULTILINE;
static uint64_t CAP_MULTILINE;
static uint64_t CAP_ECHO;
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

	/* for consistent message IDs */
	reset_msgid_for_tests();

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
roundtrip_echo_label(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE | CLICAP_LABELED_RESPONSE | CLICAP_ECHO_MESSAGE | CLICAP_MESSAGE_TAGS);
	SetServerCap(server, CAP_MULTILINE | CAP_ECHOB);

	/* the original multiline message */
	snprintf(line, sizeof(line), "@label=foo BATCH +remote draft/multiline %s" CRLF,
		remote->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=remote PRIVMSG %s :one" CRLF,
		remote->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=remote PRIVMSG %s :two" CRLF,
		remote->name);
	client_util_parse(user, line);
	client_util_parse(user, "BATCH -remote" CRLF);

	/* locally generated echo-message */
	snprintf(expected, sizeof(expected), "@msgid=" MSGID ";label=foo :%s!%s@%s BATCH +remote draft/multiline %s" CRLF,
		user->name, user->username, user->host, remote->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=remote :%s!%s@%s PRIVMSG %s :one" CRLF,
		user->name, user->username, user->host, remote->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=remote :%s!%s@%s PRIVMSG %s :two" CRLF,
		user->name, user->username, user->host, remote->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), ":%s!%s@%s BATCH -remote" CRLF,
		user->name, user->username, user->host);
	is_client_sendq(expected, user, MSG);

	/* what we're sending to the remote server */
	snprintf(expected, sizeof(expected), "@msgid=" MSGID " :%s BATCH +%s draft/multiline %s" CRLF,
		user->id, batch1, remote->id);
	is_client_sendq_one(expected, server, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s PRIVMSG %s :one" CRLF,
		batch1, user->id, remote->id);
	is_client_sendq_one(expected, server, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s PRIVMSG %s :two" CRLF,
		batch1, user->id, remote->id);
	is_client_sendq_one(expected, server, MSG);
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF,
		user->id, batch1);
	is_client_sendq(expected, server, MSG);

	/* the echo-message we might get back from the s2s link */
	snprintf(line, sizeof(line), ":%s BATCH +outer solanum.chat/echo %s" CRLF,
		remote->id, user->id);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@msgid=" MSGID ";batch=outer :%s BATCH +inner draft/multiline %s" CRLF,
		remote->id, remote->name);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=inner :%s PRIVMSG %s :one" CRLF,
		remote->id, remote->name);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=inner :%s PRIVMSG %s :two" CRLF,
		remote->id, remote->name);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=outer :%s BATCH -inner" CRLF,
		remote->id);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), ":%s BATCH -outer" CRLF,
		remote->id);
	client_util_parse(server, line);

	/* ensure the remote echo is ignored */
	is_client_sendq_empty(user, MSG);

	standard_free();
}

static void
roundtrip_echo_label__fallback(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_MULTILINE | CLICAP_LABELED_RESPONSE | CLICAP_ECHO_MESSAGE | CLICAP_MESSAGE_TAGS);
	SetServerCap(server, CAP_ECHO);

	/* the original multiline message */
	snprintf(line, sizeof(line), "@label=foo BATCH +remote draft/multiline %s" CRLF,
		remote->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=remote PRIVMSG %s :one" CRLF,
		remote->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=remote PRIVMSG %s :two" CRLF,
		remote->name);
	client_util_parse(user, line);
	snprintf(line, sizeof(line), "@batch=remote PRIVMSG %s :" CRLF,
		remote->name);
	client_util_parse(user, line);
	client_util_parse(user, "BATCH -remote" CRLF);

	/* locally generated echo-message */
	snprintf(expected, sizeof(expected), "@msgid=" MSGID ";label=foo :%s!%s@%s BATCH +remote draft/multiline %s" CRLF,
		user->name, user->username, user->host, remote->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=remote :%s!%s@%s PRIVMSG %s :one" CRLF,
		user->name, user->username, user->host, remote->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=remote :%s!%s@%s PRIVMSG %s :two" CRLF,
		user->name, user->username, user->host, remote->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), "@batch=remote :%s!%s@%s PRIVMSG %s :" CRLF,
		user->name, user->username, user->host, remote->name);
	is_client_sendq_one(expected, user, MSG);
	snprintf(expected, sizeof(expected), ":%s!%s@%s BATCH -remote" CRLF,
		user->name, user->username, user->host);
	is_client_sendq(expected, user, MSG);

	/* what we're sending to the remote server */
	snprintf(expected, sizeof(expected), "@msgid=" MSGID " :%s PRIVMSG %s :one" CRLF,
		user->id, remote->id);
	is_client_sendq_one(expected, server, MSG);
	snprintf(expected, sizeof(expected), ":%s PRIVMSG %s :two" CRLF,
		user->id, remote->id);
	is_client_sendq(expected, server, MSG);

	/* the echo-message we might get back from the s2s link */
	snprintf(line, sizeof(line), "@msgid=" MSGID " :%s ECHO P %s :one" CRLF,
		remote->id, user->id);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), ":%s ECHO P %s :two" CRLF,
		remote->id, user->id);
	client_util_parse(server, line);

	/* ensure the remote echo is ignored */
	is_client_sendq_empty(user, MSG);

	standard_free();
}

static void
roundtrip_echo_label__single(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_BATCH | CLICAP_LABELED_RESPONSE | CLICAP_ECHO_MESSAGE | CLICAP_MESSAGE_TAGS);
	SetServerCap(server, CAP_ECHO);

	snprintf(line, sizeof(line), "@label=foo PRIVMSG %s :one" CRLF,
		remote->name);
	client_util_parse(user, line);

	/* locally generated echo-message */
	snprintf(expected, sizeof(expected), "@msgid=" MSGID ";label=foo :%s!%s@%s PRIVMSG %s :one" CRLF,
		user->name, user->username, user->host, remote->name);
	is_client_sendq(expected, user, MSG);

	/* what we're sending to the remote server */
	snprintf(expected, sizeof(expected), "@msgid=" MSGID " :%s PRIVMSG %s :one" CRLF,
		user->id, remote->id);
	is_client_sendq(expected, server, MSG);

	/* the echo-message we might get back from the s2s link */
	snprintf(line, sizeof(line), "@msgid=" MSGID " :%s ECHO P %s :one" CRLF,
		remote->id, user->id);
	client_util_parse(server, line);

	/* ensure the remote echo is ignored */
	is_client_sendq_empty(user, MSG);

	standard_free();
}

static void
generate_echo(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetServerCap(server, CAP_ECHO);
	SetServerCap(server2, CAP_ECHO);

	/* generate a local echo-message even though the PRIVMSG target is remote,
	 * because we have remote labeled response (and therefore remote echoes) disabled */
	snprintf(line, sizeof(line), ":%s PRIVMSG %s :test" CRLF,
		remote->id, remote2->id);
	client_util_parse(server, line);

	snprintf(expected, sizeof(expected), ":%s ECHO P %s :test" CRLF,
		remote2->id, remote->id);
	is_client_sendq(expected, server, MSG);

	snprintf(expected, sizeof(expected), ":%s PRIVMSG %s :test" CRLF,
		remote->id, remote2->id);
	is_client_sendq(expected, server2, MSG);

	/* and swallow the ECHO generated by the actual target */
	snprintf(line, sizeof(line), ":%s ECHO P %s :test" CRLF,
		remote->id, remote2->id);
	client_util_parse(server2, line);

	is_client_sendq_empty(server, MSG);

	standard_free();
}

static void
generate_echo__multiline(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetServerCap(server, CAP_ECHO | CAP_ECHOB | CAP_MULTILINE);
	SetServerCap(server2, CAP_ECHO | CAP_ECHOB | CAP_MULTILINE);

	/* generate a local echo-message even though the PRIVMSG target is remote,
	 * because we have remote labeled response (and therefore remote echoes) disabled */
	snprintf(line, sizeof(line), ":%s BATCH +remote draft/multiline %s" CRLF,
		remote->id, remote2->id);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=remote :%s PRIVMSG %s :one" CRLF,
		remote->id, remote2->id);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), "@batch=remote :%s PRIVMSG %s :two" CRLF,
		remote->id, remote2->id);
	client_util_parse(server, line);
	snprintf(line, sizeof(line), ":%s BATCH -remote" CRLF,
		remote->id);
	client_util_parse(server, line);

	snprintf(expected, sizeof(expected), ":%s BATCH +%s solanum.chat/echo %s" CRLF,
		remote2->id, batch2, remote->id);
	is_client_sendq_one(expected, server, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s BATCH +remote draft/multiline %s" CRLF,
		batch2, remote2->id, remote2->name);
	is_client_sendq_one(expected, server, MSG);
	snprintf(expected, sizeof(expected), "@batch=remote :%s PRIVMSG %s :one" CRLF,
		remote2->id, remote2->name);
	is_client_sendq_one(expected, server, MSG);
	snprintf(expected, sizeof(expected), "@batch=remote :%s PRIVMSG %s :two" CRLF,
		remote2->id, remote2->name);
	is_client_sendq_one(expected, server, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s BATCH -remote" CRLF,
		batch2, remote2->id);
	is_client_sendq_one(expected, server, MSG);
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF,
		remote2->id, batch2);
	is_client_sendq(expected, server, MSG);

	snprintf(expected, sizeof(expected), ":%s BATCH +%s draft/multiline %s" CRLF,
		remote->id, batch1, remote2->id);
	is_client_sendq_one(expected, server2, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s PRIVMSG %s :one" CRLF,
		batch1, remote->id, remote2->id);
	is_client_sendq_one(expected, server2, MSG);
	snprintf(expected, sizeof(expected), "@batch=%s :%s PRIVMSG %s :two" CRLF,
		batch1, remote->id, remote2->id);
	is_client_sendq_one(expected, server2, MSG);
	snprintf(expected, sizeof(expected), ":%s BATCH -%s" CRLF,
		remote->id, batch1);
	is_client_sendq(expected, server2, MSG);

	/* and swallow the ECHO generated by the actual target */
	snprintf(line, sizeof(line), ":%s BATCH +%s solanum.chat/echo %s" CRLF,
		server2->id, batch2, remote->id);
	client_util_parse(server2, line);
	snprintf(line, sizeof(line), "@batch=%s :%s BATCH +%s draft/multiline %s" CRLF,
		batch2, remote2->id, batch1, remote2->id);
	client_util_parse(server2, line);
	snprintf(line, sizeof(line), "@batch=%s :%s PRIVMSG %s :one" CRLF,
		batch1, remote2->id, remote2->id);
	client_util_parse(server2, line);
	snprintf(line, sizeof(line), "@batch=%s :%s PRIVMSG %s :two" CRLF,
		batch1, remote2->id, remote2->id);
	client_util_parse(server2, line);
	snprintf(line, sizeof(line), "@batch=%s :%s BATCH -%s" CRLF,
		batch2, remote2->id, batch1);
	client_util_parse(server2, line);
	snprintf(line, sizeof(line), ":%s BATCH -%s" CRLF,
		server2->id, batch2);
	client_util_parse(server2, line);

	is_client_sendq_empty(server, MSG);

	standard_free();
}

static void
passthrough_response_tag(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();

	/* mask doesn't include us */
	snprintf(line, sizeof(line), "@solanum.chat/response=%s,batch,%s :%s WHOIS %s :%s" CRLF,
		remote->id, server2->name, remote->id, remote2->id, remote2->id);
	client_util_parse(server, line);

	snprintf(expected, sizeof(expected), "@solanum.chat/response=%s,batch,%s :%s WHOIS %s :%s" CRLF,
		remote->id, server2->name, remote->id, remote2->id, remote2->id);
	is_client_sendq(expected, server2, MSG);

	is_client_sendq_empty(server, MSG);

	/* mask includes us */
	snprintf(line, sizeof(line), "@solanum.chat/response=%s,batch,* :%s WHOIS %s :%s" CRLF,
		remote->id, remote->id, remote2->id, remote2->name);
	client_util_parse(server, line);

	snprintf(expected, sizeof(expected), "@solanum.chat/response=%s,batch,* :%s WHOIS %s :%s" CRLF,
		remote->id, remote->id, remote2->id, remote2->name);
	is_client_sendq(expected, server2, MSG);

	snprintf(expected, sizeof(expected), "@solanum.chat/response=%s,batch,* :%s ENCAP %s ACK" CRLF,
		remote->id, me.id, server->name);
	is_client_sendq(expected, server, MSG);

	standard_free();
}

static void
passthrough_response_tag__privmsg(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetServerCap(server, CAP_ECHO);
	SetServerCap(server2, CAP_ECHO);

	snprintf(line, sizeof(line), "@solanum.chat/response=%s,batch,%s :%s PRIVMSG %s :test" CRLF,
		remote->id, server2->name, remote->id, remote2->id);
	client_util_parse(server, line);

	snprintf(expected, sizeof(expected), "@solanum.chat/response=%s,batch,%s :%s ECHO P %s :test" CRLF,
		remote->id, server2->name, remote2->id, remote->id);
	is_client_sendq(expected, server, MSG);

	snprintf(expected, sizeof(expected), "@solanum.chat/response=%s,batch,%s :%s PRIVMSG %s :test" CRLF,
		remote->id, server2->name, remote->id, remote2->id);
	is_client_sendq(expected, server2, MSG);

	standard_free();
}

/* didn't have a better spot to put this... */
static void
no_remote_label(void)
{
	char expected[BUFSIZE];
	char line[BUFSIZE];

	standard_init();
	SetClientCap(user, CLICAP_LABELED_RESPONSE | CLICAP_BATCH | CLICAP_MESSAGE_TAGS);

	snprintf(line, sizeof(line), "@label=foo WHOIS %s :%s" CRLF,
		remote->name, remote->name);
	client_util_parse(user, line);

	snprintf(expected, sizeof(expected), "@label=foo :%s ACK" CRLF,
		me.name);
	is_client_sendq(expected, user, MSG);

	snprintf(expected, sizeof(expected), ":%s WHOIS %s :%s" CRLF,
		user->id, remote->id, remote->name);
	is_client_sendq(expected, server, MSG);

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
	CAP_ECHO = capability_get(serv_capindex, "ECHO", NULL);
	CAP_ECHOB = capability_get(serv_capindex, "ECHOB", NULL);
	ok(CLICAP_MULTILINE != 0, "draft/multiline client capability is loaded");
	ok(CAP_MULTILINE != 0, "MULTILN server capability is loaded");
	ok(CAP_ECHO != 0, "ECHO server capability is loaded");
	ok(CAP_ECHOB != 0, "ECHOB server capability is loaded");

	struct module *tag_message_id = findmodule_byname("tag_message_id");
	ok(tag_message_id != NULL, "tag_message_id module is loaded");

	reset_msgid_for_tests = lt_dlsym(tag_message_id->address, "reset_msgid_for_tests");
	ok(reset_msgid_for_tests != NULL, "reset_msgid_for_tests exists in tag_message_id");

	srand(0);
	generate_batch_id(batch1, sizeof(batch1));
	generate_batch_id(batch2, sizeof(batch2));
	generate_batch_id(batch3, sizeof(batch3));
	generate_batch_id(batch4, sizeof(batch4));
	generate_batch_id(batch5, sizeof(batch5));

	roundtrip_echo_label();
	roundtrip_echo_label__fallback();
	roundtrip_echo_label__single();
	generate_echo();
	generate_echo__multiline();
	passthrough_response_tag();
	passthrough_response_tag__privmsg();
	no_remote_label();

	client_util_free();
	ircd_util_free();
	return 0;
}
