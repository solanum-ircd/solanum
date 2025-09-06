/*
 *  msgbuf_parse1.c: Test msgbuf under various conditions
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

#include "stdinc.h"
#include "ircd_defs.h"
#include "msgbuf.h"
#include "client.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__

struct Client me;
static char tmp[10240];
static const char text[] =
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba"
"ZYXWVUTSRQPONMLKJIHGFEDCBAzyxwvutsrqponmlkjihgfedcba9876543210abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
;

#define MKTEXT(n) &text[sizeof(text) - ((n) + 1)]

static void basic_tags1(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void basic_tags2(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(2, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void basic_tags3(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(3, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void basic_tags4(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(4, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void basic_tags5a(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(5, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void basic_tags5b(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=val=ue0;tag1=val=ue1;tag2=val=ue2;tag3=val=ue3;tag4=val=ue4 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(5, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("val=ue0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("val=ue1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("val=ue2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("val=ue3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("val=ue4", msgbuf.tags[4].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void basic_tags13(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(13, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void basic_tags14(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(14, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void basic_tags15(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=value14 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(15, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string("value14", msgbuf.tags[14].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void basic_tags16(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=value14;tag15=value15 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(16, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string("value14", msgbuf.tags[14].value, MSG);

			is_string("tag15", msgbuf.tags[15].key, MSG);
			is_string("value15", msgbuf.tags[15].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void basic_tags30(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=value14;tag15=value15;tag16=value16;tag17=value17;tag18=value18;tag19=value19;tag20=value20;tag21=value21;tag22=value22;tag23=value23;tag24=value24;tag25=value25;tag26=value26;tag27=value27;tag28=value28;tag29=value29 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(30, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string("value14", msgbuf.tags[14].value, MSG);

			is_string("tag15", msgbuf.tags[15].key, MSG);
			is_string("value15", msgbuf.tags[15].value, MSG);

			is_string("tag16", msgbuf.tags[16].key, MSG);
			is_string("value16", msgbuf.tags[16].value, MSG);

			is_string("tag17", msgbuf.tags[17].key, MSG);
			is_string("value17", msgbuf.tags[17].value, MSG);

			is_string("tag18", msgbuf.tags[18].key, MSG);
			is_string("value18", msgbuf.tags[18].value, MSG);

			is_string("tag19", msgbuf.tags[19].key, MSG);
			is_string("value19", msgbuf.tags[19].value, MSG);

			is_string("tag20", msgbuf.tags[20].key, MSG);
			is_string("value20", msgbuf.tags[20].value, MSG);

			is_string("tag21", msgbuf.tags[21].key, MSG);
			is_string("value21", msgbuf.tags[21].value, MSG);

			is_string("tag22", msgbuf.tags[22].key, MSG);
			is_string("value22", msgbuf.tags[22].value, MSG);

			is_string("tag23", msgbuf.tags[23].key, MSG);
			is_string("value23", msgbuf.tags[23].value, MSG);

			is_string("tag24", msgbuf.tags[24].key, MSG);
			is_string("value24", msgbuf.tags[24].value, MSG);

			is_string("tag25", msgbuf.tags[25].key, MSG);
			is_string("value25", msgbuf.tags[25].value, MSG);

			is_string("tag26", msgbuf.tags[26].key, MSG);
			is_string("value26", msgbuf.tags[26].value, MSG);

			is_string("tag27", msgbuf.tags[27].key, MSG);
			is_string("value27", msgbuf.tags[27].value, MSG);

			is_string("tag28", msgbuf.tags[28].key, MSG);
			is_string("value28", msgbuf.tags[28].value, MSG);

			is_string("tag29", msgbuf.tags[29].key, MSG);
			is_string("value29", msgbuf.tags[29].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void basic_tags31(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=value14;tag15=value15;tag16=value16;tag17=value17;tag18=value18;tag19=value19;tag20=value20;tag21=value21;tag22=value22;tag23=value23;tag24=value24;tag25=value25;tag26=value26;tag27=value27;tag28=value28;tag29=value29;tag30=value30 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(30, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string("value14", msgbuf.tags[14].value, MSG);

			is_string("tag15", msgbuf.tags[15].key, MSG);
			is_string("value15", msgbuf.tags[15].value, MSG);

			is_string("tag16", msgbuf.tags[16].key, MSG);
			is_string("value16", msgbuf.tags[16].value, MSG);

			is_string("tag17", msgbuf.tags[17].key, MSG);
			is_string("value17", msgbuf.tags[17].value, MSG);

			is_string("tag18", msgbuf.tags[18].key, MSG);
			is_string("value18", msgbuf.tags[18].value, MSG);

			is_string("tag19", msgbuf.tags[19].key, MSG);
			is_string("value19", msgbuf.tags[19].value, MSG);

			is_string("tag20", msgbuf.tags[20].key, MSG);
			is_string("value20", msgbuf.tags[20].value, MSG);

			is_string("tag21", msgbuf.tags[21].key, MSG);
			is_string("value21", msgbuf.tags[21].value, MSG);

			is_string("tag22", msgbuf.tags[22].key, MSG);
			is_string("value22", msgbuf.tags[22].value, MSG);

			is_string("tag23", msgbuf.tags[23].key, MSG);
			is_string("value23", msgbuf.tags[23].value, MSG);

			is_string("tag24", msgbuf.tags[24].key, MSG);
			is_string("value24", msgbuf.tags[24].value, MSG);

			is_string("tag25", msgbuf.tags[25].key, MSG);
			is_string("value25", msgbuf.tags[25].value, MSG);

			is_string("tag26", msgbuf.tags[26].key, MSG);
			is_string("value26", msgbuf.tags[26].value, MSG);

			is_string("tag27", msgbuf.tags[27].key, MSG);
			is_string("value27", msgbuf.tags[27].value, MSG);

			is_string("tag28", msgbuf.tags[28].key, MSG);
			is_string("value28", msgbuf.tags[28].value, MSG);

			is_string("tag29", msgbuf.tags[29].key, MSG);
			is_string("value29", msgbuf.tags[29].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void basic_tags32(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=value14;tag15=value15;tag16=value16;tag17=value17;tag18=value18;tag19=value19;tag20=value20;tag21=value21;tag22=value22;tag23=value23;tag24=value24;tag25=value25;tag26=value26;tag27=value27;tag28=value28;tag29=value29;tag30=value30;tag31=value31 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(30, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string("value14", msgbuf.tags[14].value, MSG);

			is_string("tag15", msgbuf.tags[15].key, MSG);
			is_string("value15", msgbuf.tags[15].value, MSG);

			is_string("tag16", msgbuf.tags[16].key, MSG);
			is_string("value16", msgbuf.tags[16].value, MSG);

			is_string("tag17", msgbuf.tags[17].key, MSG);
			is_string("value17", msgbuf.tags[17].value, MSG);

			is_string("tag18", msgbuf.tags[18].key, MSG);
			is_string("value18", msgbuf.tags[18].value, MSG);

			is_string("tag19", msgbuf.tags[19].key, MSG);
			is_string("value19", msgbuf.tags[19].value, MSG);

			is_string("tag20", msgbuf.tags[20].key, MSG);
			is_string("value20", msgbuf.tags[20].value, MSG);

			is_string("tag21", msgbuf.tags[21].key, MSG);
			is_string("value21", msgbuf.tags[21].value, MSG);

			is_string("tag22", msgbuf.tags[22].key, MSG);
			is_string("value22", msgbuf.tags[22].value, MSG);

			is_string("tag23", msgbuf.tags[23].key, MSG);
			is_string("value23", msgbuf.tags[23].value, MSG);

			is_string("tag24", msgbuf.tags[24].key, MSG);
			is_string("value24", msgbuf.tags[24].value, MSG);

			is_string("tag25", msgbuf.tags[25].key, MSG);
			is_string("value25", msgbuf.tags[25].value, MSG);

			is_string("tag26", msgbuf.tags[26].key, MSG);
			is_string("value26", msgbuf.tags[26].value, MSG);

			is_string("tag27", msgbuf.tags[27].key, MSG);
			is_string("value27", msgbuf.tags[27].value, MSG);

			is_string("tag28", msgbuf.tags[28].key, MSG);
			is_string("value28", msgbuf.tags[28].value, MSG);

			is_string("tag29", msgbuf.tags[29].key, MSG);
			is_string("value29", msgbuf.tags[29].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void too_long_tags30a(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=value14;tag15=value15;tag16=value16;tag17=value17;tag18=value18;tag19=value19;tag20=value20;tag21=value21;tag22=value22;tag23=value23;tag24=value24;tag25=value25;tag26=value26;tag27=value27;tag28=value28;tag29=");
	strcat(tmp, MKTEXT(7797));
	is_int(8190, strlen(tmp), MSG);
	strcat(tmp, " PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(30, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string("value14", msgbuf.tags[14].value, MSG);

			is_string("tag15", msgbuf.tags[15].key, MSG);
			is_string("value15", msgbuf.tags[15].value, MSG);

			is_string("tag16", msgbuf.tags[16].key, MSG);
			is_string("value16", msgbuf.tags[16].value, MSG);

			is_string("tag17", msgbuf.tags[17].key, MSG);
			is_string("value17", msgbuf.tags[17].value, MSG);

			is_string("tag18", msgbuf.tags[18].key, MSG);
			is_string("value18", msgbuf.tags[18].value, MSG);

			is_string("tag19", msgbuf.tags[19].key, MSG);
			is_string("value19", msgbuf.tags[19].value, MSG);

			is_string("tag20", msgbuf.tags[20].key, MSG);
			is_string("value20", msgbuf.tags[20].value, MSG);

			is_string("tag21", msgbuf.tags[21].key, MSG);
			is_string("value21", msgbuf.tags[21].value, MSG);

			is_string("tag22", msgbuf.tags[22].key, MSG);
			is_string("value22", msgbuf.tags[22].value, MSG);

			is_string("tag23", msgbuf.tags[23].key, MSG);
			is_string("value23", msgbuf.tags[23].value, MSG);

			is_string("tag24", msgbuf.tags[24].key, MSG);
			is_string("value24", msgbuf.tags[24].value, MSG);

			is_string("tag25", msgbuf.tags[25].key, MSG);
			is_string("value25", msgbuf.tags[25].value, MSG);

			is_string("tag26", msgbuf.tags[26].key, MSG);
			is_string("value26", msgbuf.tags[26].value, MSG);

			is_string("tag27", msgbuf.tags[27].key, MSG);
			is_string("value27", msgbuf.tags[27].value, MSG);

			is_string("tag28", msgbuf.tags[28].key, MSG);
			is_string("value28", msgbuf.tags[28].value, MSG);

			is_string("tag29", msgbuf.tags[29].key, MSG);
			is_string(MKTEXT(7797), msgbuf.tags[29].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void too_long_tags30b(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=value14;tag15=value15;tag16=value16;tag17=value17;tag18=value18;tag19=value19;tag20=value20;tag21=value21;tag22=value22;tag23=value23;tag24=value24;tag25=value25;tag26=value26;tag27=value27;tag28=value28;tag29=");
	strcat(tmp, MKTEXT(7798)); /* final character will be replaced with ' ' */
	is_int(8191, strlen(tmp), MSG);
	strcat(tmp, "PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(30, msgbuf.n_tags, MSG)) {
			char tmp2[10240];

			/* the value will be truncated */
			strcpy(tmp2, MKTEXT(7798));
			tmp2[strlen(tmp2) - 1] = 0;

			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string("value14", msgbuf.tags[14].value, MSG);

			is_string("tag15", msgbuf.tags[15].key, MSG);
			is_string("value15", msgbuf.tags[15].value, MSG);

			is_string("tag16", msgbuf.tags[16].key, MSG);
			is_string("value16", msgbuf.tags[16].value, MSG);

			is_string("tag17", msgbuf.tags[17].key, MSG);
			is_string("value17", msgbuf.tags[17].value, MSG);

			is_string("tag18", msgbuf.tags[18].key, MSG);
			is_string("value18", msgbuf.tags[18].value, MSG);

			is_string("tag19", msgbuf.tags[19].key, MSG);
			is_string("value19", msgbuf.tags[19].value, MSG);

			is_string("tag20", msgbuf.tags[20].key, MSG);
			is_string("value20", msgbuf.tags[20].value, MSG);

			is_string("tag21", msgbuf.tags[21].key, MSG);
			is_string("value21", msgbuf.tags[21].value, MSG);

			is_string("tag22", msgbuf.tags[22].key, MSG);
			is_string("value22", msgbuf.tags[22].value, MSG);

			is_string("tag23", msgbuf.tags[23].key, MSG);
			is_string("value23", msgbuf.tags[23].value, MSG);

			is_string("tag24", msgbuf.tags[24].key, MSG);
			is_string("value24", msgbuf.tags[24].value, MSG);

			is_string("tag25", msgbuf.tags[25].key, MSG);
			is_string("value25", msgbuf.tags[25].value, MSG);

			is_string("tag26", msgbuf.tags[26].key, MSG);
			is_string("value26", msgbuf.tags[26].value, MSG);

			is_string("tag27", msgbuf.tags[27].key, MSG);
			is_string("value27", msgbuf.tags[27].value, MSG);

			is_string("tag28", msgbuf.tags[28].key, MSG);
			is_string("value28", msgbuf.tags[28].value, MSG);

			is_string("tag29", msgbuf.tags[29].key, MSG);
			is_string(tmp2, msgbuf.tags[29].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void too_long_tags31a(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=value14;tag15=value15;tag16=value16;tag17=value17;tag18=value18;tag19=value19;tag20=value20;tag21=value21;tag22=value22;tag23=value23;tag24=value24;tag25=value25;tag26=value26;tag27=value27;tag28=value28;tag29=value29;tag30=");
	strcat(tmp, MKTEXT(7783));
	is_int(8190, strlen(tmp), MSG);
	strcat(tmp, " PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(30, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string("value14", msgbuf.tags[14].value, MSG);

			is_string("tag15", msgbuf.tags[15].key, MSG);
			is_string("value15", msgbuf.tags[15].value, MSG);

			is_string("tag16", msgbuf.tags[16].key, MSG);
			is_string("value16", msgbuf.tags[16].value, MSG);

			is_string("tag17", msgbuf.tags[17].key, MSG);
			is_string("value17", msgbuf.tags[17].value, MSG);

			is_string("tag18", msgbuf.tags[18].key, MSG);
			is_string("value18", msgbuf.tags[18].value, MSG);

			is_string("tag19", msgbuf.tags[19].key, MSG);
			is_string("value19", msgbuf.tags[19].value, MSG);

			is_string("tag20", msgbuf.tags[20].key, MSG);
			is_string("value20", msgbuf.tags[20].value, MSG);

			is_string("tag21", msgbuf.tags[21].key, MSG);
			is_string("value21", msgbuf.tags[21].value, MSG);

			is_string("tag22", msgbuf.tags[22].key, MSG);
			is_string("value22", msgbuf.tags[22].value, MSG);

			is_string("tag23", msgbuf.tags[23].key, MSG);
			is_string("value23", msgbuf.tags[23].value, MSG);

			is_string("tag24", msgbuf.tags[24].key, MSG);
			is_string("value24", msgbuf.tags[24].value, MSG);

			is_string("tag25", msgbuf.tags[25].key, MSG);
			is_string("value25", msgbuf.tags[25].value, MSG);

			is_string("tag26", msgbuf.tags[26].key, MSG);
			is_string("value26", msgbuf.tags[26].value, MSG);

			is_string("tag27", msgbuf.tags[27].key, MSG);
			is_string("value27", msgbuf.tags[27].value, MSG);

			is_string("tag28", msgbuf.tags[28].key, MSG);
			is_string("value28", msgbuf.tags[28].value, MSG);

			is_string("tag29", msgbuf.tags[29].key, MSG);
			is_string("value29", msgbuf.tags[29].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void too_long_tags31b(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=value14;tag15=value15;tag16=value16;tag17=value17;tag18=value18;tag19=value19;tag20=value20;tag21=value21;tag22=value22;tag23=value23;tag24=value24;tag25=value25;tag26=value26;tag27=value27;tag28=value28;tag29=value29;tag30=");
	strcat(tmp, MKTEXT(7784)); /* final character will be replaced with ' ' */
	is_int(8191, strlen(tmp), MSG);
	strcat(tmp, "PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(30, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string("value14", msgbuf.tags[14].value, MSG);

			is_string("tag15", msgbuf.tags[15].key, MSG);
			is_string("value15", msgbuf.tags[15].value, MSG);

			is_string("tag16", msgbuf.tags[16].key, MSG);
			is_string("value16", msgbuf.tags[16].value, MSG);

			is_string("tag17", msgbuf.tags[17].key, MSG);
			is_string("value17", msgbuf.tags[17].value, MSG);

			is_string("tag18", msgbuf.tags[18].key, MSG);
			is_string("value18", msgbuf.tags[18].value, MSG);

			is_string("tag19", msgbuf.tags[19].key, MSG);
			is_string("value19", msgbuf.tags[19].value, MSG);

			is_string("tag20", msgbuf.tags[20].key, MSG);
			is_string("value20", msgbuf.tags[20].value, MSG);

			is_string("tag21", msgbuf.tags[21].key, MSG);
			is_string("value21", msgbuf.tags[21].value, MSG);

			is_string("tag22", msgbuf.tags[22].key, MSG);
			is_string("value22", msgbuf.tags[22].value, MSG);

			is_string("tag23", msgbuf.tags[23].key, MSG);
			is_string("value23", msgbuf.tags[23].value, MSG);

			is_string("tag24", msgbuf.tags[24].key, MSG);
			is_string("value24", msgbuf.tags[24].value, MSG);

			is_string("tag25", msgbuf.tags[25].key, MSG);
			is_string("value25", msgbuf.tags[25].value, MSG);

			is_string("tag26", msgbuf.tags[26].key, MSG);
			is_string("value26", msgbuf.tags[26].value, MSG);

			is_string("tag27", msgbuf.tags[27].key, MSG);
			is_string("value27", msgbuf.tags[27].value, MSG);

			is_string("tag28", msgbuf.tags[28].key, MSG);
			is_string("value28", msgbuf.tags[28].value, MSG);

			is_string("tag29", msgbuf.tags[29].key, MSG);
			is_string("value29", msgbuf.tags[29].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void too_long_tags32a(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=value14;tag15=value15;tag16=value16;tag17=value17;tag18=value18;tag19=value19;tag20=value20;tag21=value21;tag22=value22;tag23=value23;tag24=value24;tag25=value25;tag26=value26;tag27=value27;tag28=value28;tag29=value29;tag30=value30;tag31=");
	strcat(tmp, MKTEXT(7769));
	is_int(8190, strlen(tmp), MSG);
	strcat(tmp, " PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(30, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string("value14", msgbuf.tags[14].value, MSG);

			is_string("tag15", msgbuf.tags[15].key, MSG);
			is_string("value15", msgbuf.tags[15].value, MSG);

			is_string("tag16", msgbuf.tags[16].key, MSG);
			is_string("value16", msgbuf.tags[16].value, MSG);

			is_string("tag17", msgbuf.tags[17].key, MSG);
			is_string("value17", msgbuf.tags[17].value, MSG);

			is_string("tag18", msgbuf.tags[18].key, MSG);
			is_string("value18", msgbuf.tags[18].value, MSG);

			is_string("tag19", msgbuf.tags[19].key, MSG);
			is_string("value19", msgbuf.tags[19].value, MSG);

			is_string("tag20", msgbuf.tags[20].key, MSG);
			is_string("value20", msgbuf.tags[20].value, MSG);

			is_string("tag21", msgbuf.tags[21].key, MSG);
			is_string("value21", msgbuf.tags[21].value, MSG);

			is_string("tag22", msgbuf.tags[22].key, MSG);
			is_string("value22", msgbuf.tags[22].value, MSG);

			is_string("tag23", msgbuf.tags[23].key, MSG);
			is_string("value23", msgbuf.tags[23].value, MSG);

			is_string("tag24", msgbuf.tags[24].key, MSG);
			is_string("value24", msgbuf.tags[24].value, MSG);

			is_string("tag25", msgbuf.tags[25].key, MSG);
			is_string("value25", msgbuf.tags[25].value, MSG);

			is_string("tag26", msgbuf.tags[26].key, MSG);
			is_string("value26", msgbuf.tags[26].value, MSG);

			is_string("tag27", msgbuf.tags[27].key, MSG);
			is_string("value27", msgbuf.tags[27].value, MSG);

			is_string("tag28", msgbuf.tags[28].key, MSG);
			is_string("value28", msgbuf.tags[28].value, MSG);

			is_string("tag29", msgbuf.tags[29].key, MSG);
			is_string("value29", msgbuf.tags[29].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void too_long_tags32b(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=value14;tag15=value15;tag16=value16;tag17=value17;tag18=value18;tag19=value19;tag20=value20;tag21=value21;tag22=value22;tag23=value23;tag24=value24;tag25=value25;tag26=value26;tag27=value27;tag28=value28;tag29=value29;tag30=value30;tag31=");
	strcat(tmp, MKTEXT(7770)); /* final character will be replaced with ' ' */
	is_int(8191, strlen(tmp), MSG);
	strcat(tmp, "PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(30, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string("value14", msgbuf.tags[14].value, MSG);

			is_string("tag15", msgbuf.tags[15].key, MSG);
			is_string("value15", msgbuf.tags[15].value, MSG);

			is_string("tag16", msgbuf.tags[16].key, MSG);
			is_string("value16", msgbuf.tags[16].value, MSG);

			is_string("tag17", msgbuf.tags[17].key, MSG);
			is_string("value17", msgbuf.tags[17].value, MSG);

			is_string("tag18", msgbuf.tags[18].key, MSG);
			is_string("value18", msgbuf.tags[18].value, MSG);

			is_string("tag19", msgbuf.tags[19].key, MSG);
			is_string("value19", msgbuf.tags[19].value, MSG);

			is_string("tag20", msgbuf.tags[20].key, MSG);
			is_string("value20", msgbuf.tags[20].value, MSG);

			is_string("tag21", msgbuf.tags[21].key, MSG);
			is_string("value21", msgbuf.tags[21].value, MSG);

			is_string("tag22", msgbuf.tags[22].key, MSG);
			is_string("value22", msgbuf.tags[22].value, MSG);

			is_string("tag23", msgbuf.tags[23].key, MSG);
			is_string("value23", msgbuf.tags[23].value, MSG);

			is_string("tag24", msgbuf.tags[24].key, MSG);
			is_string("value24", msgbuf.tags[24].value, MSG);

			is_string("tag25", msgbuf.tags[25].key, MSG);
			is_string("value25", msgbuf.tags[25].value, MSG);

			is_string("tag26", msgbuf.tags[26].key, MSG);
			is_string("value26", msgbuf.tags[26].value, MSG);

			is_string("tag27", msgbuf.tags[27].key, MSG);
			is_string("value27", msgbuf.tags[27].value, MSG);

			is_string("tag28", msgbuf.tags[28].key, MSG);
			is_string("value28", msgbuf.tags[28].value, MSG);

			is_string("tag29", msgbuf.tags[29].key, MSG);
			is_string("value29", msgbuf.tags[29].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void empty_value1(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag= PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void empty_value2(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string(NULL, msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void malformed1a(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@=value PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		is_int(0, msgbuf.n_tags, MSG);

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void malformed2a(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@= PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		is_int(0, msgbuf.n_tags, MSG);

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void malformed1b(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@=value;tag2=value2 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag2", msgbuf.tags[0].key, MSG);
			is_string("value2", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void malformed2b(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@=;tag2=value2 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag2", msgbuf.tags[0].key, MSG);
			is_string("value2", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void malformed3(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@ta g=value PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("ta", msgbuf.tags[0].key, MSG);
			is_string(NULL, msgbuf.tags[0].value, MSG);
		}

		is_string("g=value", msgbuf.cmd, MSG);
		if (is_int(4, msgbuf.n_para, MSG)) {
			is_string("g=value", msgbuf.para[0], MSG);
			is_string("PRIVMSG", msgbuf.para[1], MSG);
			is_string("#test", msgbuf.para[2], MSG);
			is_string("test", msgbuf.para[3], MSG);
		}
	}
}

static void malformed4(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=va lue PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("va", msgbuf.tags[0].value, MSG);
		}

		is_string("lue", msgbuf.cmd, MSG);
		if (is_int(4, msgbuf.n_para, MSG)) {
			is_string("lue", msgbuf.para[0], MSG);
			is_string("PRIVMSG", msgbuf.para[1], MSG);
			is_string("#test", msgbuf.para[2], MSG);
			is_string("test", msgbuf.para[3], MSG);
		}
	}
}

static void malformed5(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@ta g=va lue PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("ta", msgbuf.tags[0].key, MSG);
			is_string(NULL, msgbuf.tags[0].value, MSG);
		}

		is_string("g=va", msgbuf.cmd, MSG);
		if (is_int(5, msgbuf.n_para, MSG)) {
			is_string("g=va", msgbuf.para[0], MSG);
			is_string("lue", msgbuf.para[1], MSG);
			is_string("PRIVMSG", msgbuf.para[2], MSG);
			is_string("#test", msgbuf.para[3], MSG);
			is_string("test", msgbuf.para[4], MSG);
		}
	}
}

static void malformed6(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag =value PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string(NULL, msgbuf.tags[0].value, MSG);
		}

		is_string("=value", msgbuf.cmd, MSG);
		if (is_int(4, msgbuf.n_para, MSG)) {
			is_string("=value", msgbuf.para[0], MSG);
			is_string("PRIVMSG", msgbuf.para[1], MSG);
			is_string("#test", msgbuf.para[2], MSG);
			is_string("test", msgbuf.para[3], MSG);
		}
	}
}

static void malformed7(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag= value PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("", msgbuf.tags[0].value, MSG);
		}

		is_string("value", msgbuf.cmd, MSG);
		if (is_int(4, msgbuf.n_para, MSG)) {
			is_string("value", msgbuf.para[0], MSG);
			is_string("PRIVMSG", msgbuf.para[1], MSG);
			is_string("#test", msgbuf.para[2], MSG);
			is_string("test", msgbuf.para[3], MSG);
		}
	}
}

static void long_tag1(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@");
	strcat(tmp, MKTEXT(509));
	strcat(tmp, "= PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string(MKTEXT(509), msgbuf.tags[0].key, MSG);
			is_string("", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void long_tag2(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@");
	strcat(tmp, MKTEXT(510));
	strcat(tmp, " PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string(MKTEXT(510), msgbuf.tags[0].key, MSG);
			is_string(NULL, msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void long_value1(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@");
	strcat(tmp, MKTEXT(1));
	strcat(tmp, "=");
	strcat(tmp, MKTEXT(508));
	strcat(tmp, " PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string(MKTEXT(1), msgbuf.tags[0].key, MSG);
			is_string(MKTEXT(508), msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void too_long_tag1(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@");
	strcat(tmp, MKTEXT(8189));
	is_int(8190, strlen(tmp), MSG);
	/* the '=' will be replaced with a ' ' when parsing */
	strcat(tmp, "=PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string(MKTEXT(8189), msgbuf.tags[0].key, MSG);
			is_string(NULL, msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void too_long_tag2(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@");
	strcat(tmp, MKTEXT(8190)); /* the last byte will be replaced with a ' ' when parsing */
	is_int(8191, strlen(tmp), MSG);
	strcat(tmp, "PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		char tmp2[10240];

		/* the tag will be truncated */
		strcpy(tmp2, MKTEXT(8190));
		tmp2[strlen(tmp2) - 1] = 0;

		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string(tmp2, msgbuf.tags[0].key, MSG);
			is_string(NULL, msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void too_long_tag3(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@");
	strcat(tmp, MKTEXT(8189));
	is_int(8190, strlen(tmp), MSG);
	/* the ';' will be replaced with a ' ' when parsing */
	strcat(tmp, ";PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string(MKTEXT(8189), msgbuf.tags[0].key, MSG);
			is_string(NULL, msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void too_long_value1(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@");
	strcat(tmp, MKTEXT(1));
	strcat(tmp, "=");
	strcat(tmp, MKTEXT(8187));
	is_int(8190, strlen(tmp), MSG);
	/* the ';' will be replaced with a ' ' when parsing */
	strcat(tmp, ";PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string(MKTEXT(1), msgbuf.tags[0].key, MSG);
			is_string(MKTEXT(8187), msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void too_long_value2(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@");
	strcat(tmp, MKTEXT(1));
	strcat(tmp, "=");
	strcat(tmp, MKTEXT(8188)); /* the last byte will be replaced with a ' ' when parsing */
	is_int(8191, strlen(tmp), MSG);
	strcat(tmp, "PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		char tmp2[10240];

		/* the value will be truncated */
		strcpy(tmp2, MKTEXT(8188));
		tmp2[strlen(tmp2) - 1] = 0;

		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string(MKTEXT(1), msgbuf.tags[0].key, MSG);
			is_string(tmp2, msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void too_long_value3(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@");
	strcat(tmp, MKTEXT(1));
	strcat(tmp, "=");
	strcat(tmp, MKTEXT(8189)); /* the second-last byte will be replaced with a ' ' when parsing */
	is_int(8192, strlen(tmp), MSG);
	strcat(tmp, "PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		char tmp2[10240];

		/* the value will be truncated */
		strcpy(tmp2, MKTEXT(8189));
		tmp2[strlen(tmp2) - 2] = 0;

		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string(MKTEXT(1), msgbuf.tags[0].key, MSG);
			is_string(tmp2, msgbuf.tags[0].value, MSG);
		}

		is_string("ZPRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("ZPRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void basic_para3a(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test :");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("", msgbuf.para[2], MSG);
		}
	}
}

static void basic_para3b(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test :test D E F G H I J K");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test D E F G H I J K", msgbuf.para[2], MSG);
		}
	}
}

static void basic_para13(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test test D E F G H I J K L M");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(13, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
			is_string("D", msgbuf.para[3], MSG);
			is_string("E", msgbuf.para[4], MSG);
			is_string("F", msgbuf.para[5], MSG);
			is_string("G", msgbuf.para[6], MSG);
			is_string("H", msgbuf.para[7], MSG);
			is_string("I", msgbuf.para[8], MSG);
			is_string("J", msgbuf.para[9], MSG);
			is_string("K", msgbuf.para[10], MSG);
			is_string("L", msgbuf.para[11], MSG);
			is_string("M", msgbuf.para[12], MSG);
		}
	}
}

static void basic_para14a(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test test D E F G H I J K L M N");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(14, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
			is_string("D", msgbuf.para[3], MSG);
			is_string("E", msgbuf.para[4], MSG);
			is_string("F", msgbuf.para[5], MSG);
			is_string("G", msgbuf.para[6], MSG);
			is_string("H", msgbuf.para[7], MSG);
			is_string("I", msgbuf.para[8], MSG);
			is_string("J", msgbuf.para[9], MSG);
			is_string("K", msgbuf.para[10], MSG);
			is_string("L", msgbuf.para[11], MSG);
			is_string("M", msgbuf.para[12], MSG);
			is_string("N", msgbuf.para[13], MSG);
		}
	}
}

static void basic_para14b(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test test D E F G H I J K L M :N");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(14, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
			is_string("D", msgbuf.para[3], MSG);
			is_string("E", msgbuf.para[4], MSG);
			is_string("F", msgbuf.para[5], MSG);
			is_string("G", msgbuf.para[6], MSG);
			is_string("H", msgbuf.para[7], MSG);
			is_string("I", msgbuf.para[8], MSG);
			is_string("J", msgbuf.para[9], MSG);
			is_string("K", msgbuf.para[10], MSG);
			is_string("L", msgbuf.para[11], MSG);
			is_string("M", msgbuf.para[12], MSG);
			is_string("N", msgbuf.para[13], MSG);
		}
	}
}

static void basic_para14c(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test test D E F G H I J K L M :N O P");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(14, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
			is_string("D", msgbuf.para[3], MSG);
			is_string("E", msgbuf.para[4], MSG);
			is_string("F", msgbuf.para[5], MSG);
			is_string("G", msgbuf.para[6], MSG);
			is_string("H", msgbuf.para[7], MSG);
			is_string("I", msgbuf.para[8], MSG);
			is_string("J", msgbuf.para[9], MSG);
			is_string("K", msgbuf.para[10], MSG);
			is_string("L", msgbuf.para[11], MSG);
			is_string("M", msgbuf.para[12], MSG);
			is_string("N O P", msgbuf.para[13], MSG);
		}
	}
}

static void basic_para15a(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test test D E F G H I J K L M N O");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
			is_string("D", msgbuf.para[3], MSG);
			is_string("E", msgbuf.para[4], MSG);
			is_string("F", msgbuf.para[5], MSG);
			is_string("G", msgbuf.para[6], MSG);
			is_string("H", msgbuf.para[7], MSG);
			is_string("I", msgbuf.para[8], MSG);
			is_string("J", msgbuf.para[9], MSG);
			is_string("K", msgbuf.para[10], MSG);
			is_string("L", msgbuf.para[11], MSG);
			is_string("M", msgbuf.para[12], MSG);
			is_string("N", msgbuf.para[13], MSG);
			is_string("O", msgbuf.para[14], MSG);
		}
	}
}

static void basic_para15b(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test test D E F G H I J K L M N :O");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
			is_string("D", msgbuf.para[3], MSG);
			is_string("E", msgbuf.para[4], MSG);
			is_string("F", msgbuf.para[5], MSG);
			is_string("G", msgbuf.para[6], MSG);
			is_string("H", msgbuf.para[7], MSG);
			is_string("I", msgbuf.para[8], MSG);
			is_string("J", msgbuf.para[9], MSG);
			is_string("K", msgbuf.para[10], MSG);
			is_string("L", msgbuf.para[11], MSG);
			is_string("M", msgbuf.para[12], MSG);
			is_string("N", msgbuf.para[13], MSG);
			is_string("O", msgbuf.para[14], MSG);
		}
	}
}

static void basic_para15c(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test test D E F G H I J K L M N O ");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
			is_string("D", msgbuf.para[3], MSG);
			is_string("E", msgbuf.para[4], MSG);
			is_string("F", msgbuf.para[5], MSG);
			is_string("G", msgbuf.para[6], MSG);
			is_string("H", msgbuf.para[7], MSG);
			is_string("I", msgbuf.para[8], MSG);
			is_string("J", msgbuf.para[9], MSG);
			is_string("K", msgbuf.para[10], MSG);
			is_string("L", msgbuf.para[11], MSG);
			is_string("M", msgbuf.para[12], MSG);
			is_string("N", msgbuf.para[13], MSG);
			is_string("O ", msgbuf.para[14], MSG);
		}
	}
}

static void basic_para15d(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test test D E F G H I J K L M N :O ");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
			is_string("D", msgbuf.para[3], MSG);
			is_string("E", msgbuf.para[4], MSG);
			is_string("F", msgbuf.para[5], MSG);
			is_string("G", msgbuf.para[6], MSG);
			is_string("H", msgbuf.para[7], MSG);
			is_string("I", msgbuf.para[8], MSG);
			is_string("J", msgbuf.para[9], MSG);
			is_string("K", msgbuf.para[10], MSG);
			is_string("L", msgbuf.para[11], MSG);
			is_string("M", msgbuf.para[12], MSG);
			is_string("N", msgbuf.para[13], MSG);
			is_string("O ", msgbuf.para[14], MSG);
		}
	}
}

static void basic_para16a(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test test D E F G H I J K L M N O P");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
			is_string("D", msgbuf.para[3], MSG);
			is_string("E", msgbuf.para[4], MSG);
			is_string("F", msgbuf.para[5], MSG);
			is_string("G", msgbuf.para[6], MSG);
			is_string("H", msgbuf.para[7], MSG);
			is_string("I", msgbuf.para[8], MSG);
			is_string("J", msgbuf.para[9], MSG);
			is_string("K", msgbuf.para[10], MSG);
			is_string("L", msgbuf.para[11], MSG);
			is_string("M", msgbuf.para[12], MSG);
			is_string("N", msgbuf.para[13], MSG);
			is_string("O P", msgbuf.para[14], MSG);
		}
	}
}

static void basic_para16b(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test test D E F G H I J K L M N O :P");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
			is_string("D", msgbuf.para[3], MSG);
			is_string("E", msgbuf.para[4], MSG);
			is_string("F", msgbuf.para[5], MSG);
			is_string("G", msgbuf.para[6], MSG);
			is_string("H", msgbuf.para[7], MSG);
			is_string("I", msgbuf.para[8], MSG);
			is_string("J", msgbuf.para[9], MSG);
			is_string("K", msgbuf.para[10], MSG);
			is_string("L", msgbuf.para[11], MSG);
			is_string("M", msgbuf.para[12], MSG);
			is_string("N", msgbuf.para[13], MSG);
			is_string("O :P", msgbuf.para[14], MSG);
		}
	}
}

static void basic_para16c(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test test D E F G H I J K L M N :O P");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
			is_string("D", msgbuf.para[3], MSG);
			is_string("E", msgbuf.para[4], MSG);
			is_string("F", msgbuf.para[5], MSG);
			is_string("G", msgbuf.para[6], MSG);
			is_string("H", msgbuf.para[7], MSG);
			is_string("I", msgbuf.para[8], MSG);
			is_string("J", msgbuf.para[9], MSG);
			is_string("K", msgbuf.para[10], MSG);
			is_string("L", msgbuf.para[11], MSG);
			is_string("M", msgbuf.para[12], MSG);
			is_string("N", msgbuf.para[13], MSG);
			is_string("O P", msgbuf.para[14], MSG);
		}
	}
}

static void basic_para17a(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test test D E F G H I J K L M N O P Q");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
			is_string("D", msgbuf.para[3], MSG);
			is_string("E", msgbuf.para[4], MSG);
			is_string("F", msgbuf.para[5], MSG);
			is_string("G", msgbuf.para[6], MSG);
			is_string("H", msgbuf.para[7], MSG);
			is_string("I", msgbuf.para[8], MSG);
			is_string("J", msgbuf.para[9], MSG);
			is_string("K", msgbuf.para[10], MSG);
			is_string("L", msgbuf.para[11], MSG);
			is_string("M", msgbuf.para[12], MSG);
			is_string("N", msgbuf.para[13], MSG);
			is_string("O P Q", msgbuf.para[14], MSG);
		}
	}
}

static void basic_para17b(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test test D E F G H I J K L M N O P :Q");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
			is_string("D", msgbuf.para[3], MSG);
			is_string("E", msgbuf.para[4], MSG);
			is_string("F", msgbuf.para[5], MSG);
			is_string("G", msgbuf.para[6], MSG);
			is_string("H", msgbuf.para[7], MSG);
			is_string("I", msgbuf.para[8], MSG);
			is_string("J", msgbuf.para[9], MSG);
			is_string("K", msgbuf.para[10], MSG);
			is_string("L", msgbuf.para[11], MSG);
			is_string("M", msgbuf.para[12], MSG);
			is_string("N", msgbuf.para[13], MSG);
			is_string("O P :Q", msgbuf.para[14], MSG);
		}
	}
}

static void basic_para17c(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test test D E F G H I J K L M N O :P Q");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
			is_string("D", msgbuf.para[3], MSG);
			is_string("E", msgbuf.para[4], MSG);
			is_string("F", msgbuf.para[5], MSG);
			is_string("G", msgbuf.para[6], MSG);
			is_string("H", msgbuf.para[7], MSG);
			is_string("I", msgbuf.para[8], MSG);
			is_string("J", msgbuf.para[9], MSG);
			is_string("K", msgbuf.para[10], MSG);
			is_string("L", msgbuf.para[11], MSG);
			is_string("M", msgbuf.para[12], MSG);
			is_string("N", msgbuf.para[13], MSG);
			is_string("O :P Q", msgbuf.para[14], MSG);
		}
	}
}

static void basic_para17d(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test test D E F G H I J K L M N :O P Q");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
			is_string("D", msgbuf.para[3], MSG);
			is_string("E", msgbuf.para[4], MSG);
			is_string("F", msgbuf.para[5], MSG);
			is_string("G", msgbuf.para[6], MSG);
			is_string("H", msgbuf.para[7], MSG);
			is_string("I", msgbuf.para[8], MSG);
			is_string("J", msgbuf.para[9], MSG);
			is_string("K", msgbuf.para[10], MSG);
			is_string("L", msgbuf.para[11], MSG);
			is_string("M", msgbuf.para[12], MSG);
			is_string("N", msgbuf.para[13], MSG);
			is_string("O P Q", msgbuf.para[14], MSG);
		}
	}
}

static void long_para1(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test ");
	strcat(tmp, MKTEXT(496));
	is_int(510, strlen(strchr(tmp, ' ')+1), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string(MKTEXT(496), msgbuf.para[2], MSG);
		}
	}
}

static void long_para2(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test ");
	strcat(tmp, MKTEXT(494));
	strcat(tmp, " :");
	is_int(510, strlen(strchr(tmp, ' ')+1), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(4, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string(MKTEXT(494), msgbuf.para[2], MSG);
			is_string("", msgbuf.para[3], MSG);
		}
	}
}

static void long_para3(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test ");
	strcat(tmp, MKTEXT(495));
	strcat(tmp, " ");
	is_int(510, strlen(strchr(tmp, ' ')+1), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string(MKTEXT(495), msgbuf.para[2], MSG);
		}
	}
}

static void long_para4(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test ");
	strcat(tmp, MKTEXT(493));
	strcat(tmp, " :x");
	is_int(510, strlen(strchr(tmp, ' ')+1), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(4, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string(MKTEXT(493), msgbuf.para[2], MSG);
			is_string("x", msgbuf.para[3], MSG);
		}
	}
}

static void too_long_para1(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test :");
	strcat(tmp, MKTEXT(496));
	is_int(511, strlen(strchr(tmp, ' ')+1), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		char tmp2[2048];

		/* the text will be truncated */
		strcpy(tmp2, MKTEXT(496));
		tmp2[strlen(tmp2) - 1] = 0;

		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string(tmp2, msgbuf.para[2], MSG);
		}
	}
}

static void too_long_para2(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value PRIVMSG #test :");
	strcat(tmp, MKTEXT(497));
	is_int(512, strlen(strchr(tmp, ' ')+1), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		char tmp2[2048];

		/* the text will be truncated */
		strcpy(tmp2, MKTEXT(497));
		tmp2[strlen(tmp2) - 2] = 0;

		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string(tmp2, msgbuf.para[2], MSG);
		}
	}
}

static void long_everything1(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=");
	strcat(tmp, MKTEXT(328));
	strcat(tmp, " PRIVMSG #test A B C D E F G H I J K L ");
	strcat(tmp, MKTEXT(472));
	is_int(512+510, strlen(tmp), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(15, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string(MKTEXT(328), msgbuf.tags[14].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("A", msgbuf.para[2], MSG);
			is_string("B", msgbuf.para[3], MSG);
			is_string("C", msgbuf.para[4], MSG);
			is_string("D", msgbuf.para[5], MSG);
			is_string("E", msgbuf.para[6], MSG);
			is_string("F", msgbuf.para[7], MSG);
			is_string("G", msgbuf.para[8], MSG);
			is_string("H", msgbuf.para[9], MSG);
			is_string("I", msgbuf.para[10], MSG);
			is_string("J", msgbuf.para[11], MSG);
			is_string("K", msgbuf.para[12], MSG);
			is_string("L", msgbuf.para[13], MSG);
			is_string(MKTEXT(472), msgbuf.para[14], MSG);
		}
	}
}

static void too_long_everything1(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=");
	strcat(tmp, MKTEXT(328));
	strcat(tmp, " PRIVMSG #test A B C D E F G H I J K L ");
	strcat(tmp, MKTEXT(472));
	strcat(tmp, "X");
	is_int(512+511, strlen(tmp), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(15, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string(MKTEXT(328), msgbuf.tags[14].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("A", msgbuf.para[2], MSG);
			is_string("B", msgbuf.para[3], MSG);
			is_string("C", msgbuf.para[4], MSG);
			is_string("D", msgbuf.para[5], MSG);
			is_string("E", msgbuf.para[6], MSG);
			is_string("F", msgbuf.para[7], MSG);
			is_string("G", msgbuf.para[8], MSG);
			is_string("H", msgbuf.para[9], MSG);
			is_string("I", msgbuf.para[10], MSG);
			is_string("J", msgbuf.para[11], MSG);
			is_string("K", msgbuf.para[12], MSG);
			is_string("L", msgbuf.para[13], MSG);
			is_string(MKTEXT(472), msgbuf.para[14], MSG);
		}
	}
}

static void long_everything2(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=");
	strcat(tmp, MKTEXT(328));
	strcat(tmp, " PRIVMSG #test A B C D E F G H I J K ");
	strcat(tmp, MKTEXT(472));
	strcat(tmp, " :");
	is_int(512+510, strlen(tmp), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(15, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string(MKTEXT(328), msgbuf.tags[14].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("A", msgbuf.para[2], MSG);
			is_string("B", msgbuf.para[3], MSG);
			is_string("C", msgbuf.para[4], MSG);
			is_string("D", msgbuf.para[5], MSG);
			is_string("E", msgbuf.para[6], MSG);
			is_string("F", msgbuf.para[7], MSG);
			is_string("G", msgbuf.para[8], MSG);
			is_string("H", msgbuf.para[9], MSG);
			is_string("I", msgbuf.para[10], MSG);
			is_string("J", msgbuf.para[11], MSG);
			is_string("K", msgbuf.para[12], MSG);
			is_string(MKTEXT(472), msgbuf.para[13], MSG);
			is_string("", msgbuf.para[14], MSG);
		}
	}
}

static void too_long_everything2(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=");
	strcat(tmp, MKTEXT(328));
	strcat(tmp, " PRIVMSG #test A B C D E F G H I J K ");
	strcat(tmp, MKTEXT(472));
	strcat(tmp, " :");
	strcat(tmp, "X");
	is_int(512+511, strlen(tmp), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(15, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string(MKTEXT(328), msgbuf.tags[14].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("A", msgbuf.para[2], MSG);
			is_string("B", msgbuf.para[3], MSG);
			is_string("C", msgbuf.para[4], MSG);
			is_string("D", msgbuf.para[5], MSG);
			is_string("E", msgbuf.para[6], MSG);
			is_string("F", msgbuf.para[7], MSG);
			is_string("G", msgbuf.para[8], MSG);
			is_string("H", msgbuf.para[9], MSG);
			is_string("I", msgbuf.para[10], MSG);
			is_string("J", msgbuf.para[11], MSG);
			is_string("K", msgbuf.para[12], MSG);
			is_string(MKTEXT(472), msgbuf.para[13], MSG);
			is_string("", msgbuf.para[14], MSG);
		}
	}
}

static void long_everything3a(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=");
	strcat(tmp, MKTEXT(328));
	strcat(tmp, " PRIVMSG #test A B C D E F G H I J K L ");
	strcat(tmp, MKTEXT(471));
	strcat(tmp, " ");
	is_int(512+510, strlen(tmp), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		char tmp2[2048];

		/* last para has a space at the end */
		memset(tmp2, 0, sizeof(tmp2));
		strcat(tmp2, MKTEXT(471));
		strcat(tmp2, " ");

		if (is_int(15, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string(MKTEXT(328), msgbuf.tags[14].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("A", msgbuf.para[2], MSG);
			is_string("B", msgbuf.para[3], MSG);
			is_string("C", msgbuf.para[4], MSG);
			is_string("D", msgbuf.para[5], MSG);
			is_string("E", msgbuf.para[6], MSG);
			is_string("F", msgbuf.para[7], MSG);
			is_string("G", msgbuf.para[8], MSG);
			is_string("H", msgbuf.para[9], MSG);
			is_string("I", msgbuf.para[10], MSG);
			is_string("J", msgbuf.para[11], MSG);
			is_string("K", msgbuf.para[12], MSG);
			is_string("L", msgbuf.para[13], MSG);
			is_string(tmp2, msgbuf.para[14], MSG);
		}
	}
}

static void too_long_everything3a(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=");
	strcat(tmp, MKTEXT(328));
	strcat(tmp, " PRIVMSG #test A B C D E F G H I J K L ");
	strcat(tmp, MKTEXT(471));
	strcat(tmp, " ");
	strcat(tmp, "X");
	is_int(512+511, strlen(tmp), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		char tmp2[2048];

		/* last para has a space at the end */
		memset(tmp2, 0, sizeof(tmp2));
		strcat(tmp2, MKTEXT(471));
		strcat(tmp2, " ");

		if (is_int(15, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string(MKTEXT(328), msgbuf.tags[14].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("A", msgbuf.para[2], MSG);
			is_string("B", msgbuf.para[3], MSG);
			is_string("C", msgbuf.para[4], MSG);
			is_string("D", msgbuf.para[5], MSG);
			is_string("E", msgbuf.para[6], MSG);
			is_string("F", msgbuf.para[7], MSG);
			is_string("G", msgbuf.para[8], MSG);
			is_string("H", msgbuf.para[9], MSG);
			is_string("I", msgbuf.para[10], MSG);
			is_string("J", msgbuf.para[11], MSG);
			is_string("K", msgbuf.para[12], MSG);
			is_string("L", msgbuf.para[13], MSG);
			is_string(tmp2, msgbuf.para[14], MSG);
		}
	}
}

static void long_everything3b(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=");
	strcat(tmp, MKTEXT(328));
	strcat(tmp, " PRIVMSG #test A B C D E F G H I J K ");
	strcat(tmp, MKTEXT(473));
	strcat(tmp, " ");
	is_int(512+510, strlen(tmp), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(15, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string(MKTEXT(328), msgbuf.tags[14].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(14, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("A", msgbuf.para[2], MSG);
			is_string("B", msgbuf.para[3], MSG);
			is_string("C", msgbuf.para[4], MSG);
			is_string("D", msgbuf.para[5], MSG);
			is_string("E", msgbuf.para[6], MSG);
			is_string("F", msgbuf.para[7], MSG);
			is_string("G", msgbuf.para[8], MSG);
			is_string("H", msgbuf.para[9], MSG);
			is_string("I", msgbuf.para[10], MSG);
			is_string("J", msgbuf.para[11], MSG);
			is_string("K", msgbuf.para[12], MSG);
			is_string(MKTEXT(473), msgbuf.para[13], MSG);
		}
	}
}

static void too_long_everything3b(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=");
	strcat(tmp, MKTEXT(328));
	strcat(tmp, " PRIVMSG #test A B C D E F G H I J K ");
	strcat(tmp, MKTEXT(473));
	strcat(tmp, " ");
	strcat(tmp, "X");
	is_int(512+511, strlen(tmp), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(15, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string(MKTEXT(328), msgbuf.tags[14].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(14, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("A", msgbuf.para[2], MSG);
			is_string("B", msgbuf.para[3], MSG);
			is_string("C", msgbuf.para[4], MSG);
			is_string("D", msgbuf.para[5], MSG);
			is_string("E", msgbuf.para[6], MSG);
			is_string("F", msgbuf.para[7], MSG);
			is_string("G", msgbuf.para[8], MSG);
			is_string("H", msgbuf.para[9], MSG);
			is_string("I", msgbuf.para[10], MSG);
			is_string("J", msgbuf.para[11], MSG);
			is_string("K", msgbuf.para[12], MSG);
			is_string(MKTEXT(473), msgbuf.para[13], MSG);
		}
	}
}

static void long_everything4(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=");
	strcat(tmp, MKTEXT(328));
	strcat(tmp, " PRIVMSG #test A B C D E F G H I J K ");
	strcat(tmp, MKTEXT(471));
	strcat(tmp, " :L");
	is_int(512+510, strlen(tmp), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(15, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string(MKTEXT(328), msgbuf.tags[14].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("A", msgbuf.para[2], MSG);
			is_string("B", msgbuf.para[3], MSG);
			is_string("C", msgbuf.para[4], MSG);
			is_string("D", msgbuf.para[5], MSG);
			is_string("E", msgbuf.para[6], MSG);
			is_string("F", msgbuf.para[7], MSG);
			is_string("G", msgbuf.para[8], MSG);
			is_string("H", msgbuf.para[9], MSG);
			is_string("I", msgbuf.para[10], MSG);
			is_string("J", msgbuf.para[11], MSG);
			is_string("K", msgbuf.para[12], MSG);
			is_string(MKTEXT(471), msgbuf.para[13], MSG);
			is_string("L", msgbuf.para[14], MSG);
		}
	}
}

static void too_long_everything4(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag0=value0;tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=");
	strcat(tmp, MKTEXT(328));
	strcat(tmp, " PRIVMSG #test A B C D E F G H I J K ");
	strcat(tmp, MKTEXT(471));
	strcat(tmp, " :L");
	strcat(tmp, "X");
	is_int(512+511, strlen(tmp), MSG);

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(15, msgbuf.n_tags, MSG)) {
			is_string("tag0", msgbuf.tags[0].key, MSG);
			is_string("value0", msgbuf.tags[0].value, MSG);

			is_string("tag1", msgbuf.tags[1].key, MSG);
			is_string("value1", msgbuf.tags[1].value, MSG);

			is_string("tag2", msgbuf.tags[2].key, MSG);
			is_string("value2", msgbuf.tags[2].value, MSG);

			is_string("tag3", msgbuf.tags[3].key, MSG);
			is_string("value3", msgbuf.tags[3].value, MSG);

			is_string("tag4", msgbuf.tags[4].key, MSG);
			is_string("value4", msgbuf.tags[4].value, MSG);

			is_string("tag5", msgbuf.tags[5].key, MSG);
			is_string("value5", msgbuf.tags[5].value, MSG);

			is_string("tag6", msgbuf.tags[6].key, MSG);
			is_string("value6", msgbuf.tags[6].value, MSG);

			is_string("tag7", msgbuf.tags[7].key, MSG);
			is_string("value7", msgbuf.tags[7].value, MSG);

			is_string("tag8", msgbuf.tags[8].key, MSG);
			is_string("value8", msgbuf.tags[8].value, MSG);

			is_string("tag9", msgbuf.tags[9].key, MSG);
			is_string("value9", msgbuf.tags[9].value, MSG);

			is_string("tag10", msgbuf.tags[10].key, MSG);
			is_string("value10", msgbuf.tags[10].value, MSG);

			is_string("tag11", msgbuf.tags[11].key, MSG);
			is_string("value11", msgbuf.tags[11].value, MSG);

			is_string("tag12", msgbuf.tags[12].key, MSG);
			is_string("value12", msgbuf.tags[12].value, MSG);

			is_string("tag13", msgbuf.tags[13].key, MSG);
			is_string("value13", msgbuf.tags[13].value, MSG);

			is_string("tag14", msgbuf.tags[14].key, MSG);
			is_string(MKTEXT(328), msgbuf.tags[14].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(15, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("A", msgbuf.para[2], MSG);
			is_string("B", msgbuf.para[3], MSG);
			is_string("C", msgbuf.para[4], MSG);
			is_string("D", msgbuf.para[5], MSG);
			is_string("E", msgbuf.para[6], MSG);
			is_string("F", msgbuf.para[7], MSG);
			is_string("G", msgbuf.para[8], MSG);
			is_string("H", msgbuf.para[9], MSG);
			is_string("I", msgbuf.para[10], MSG);
			is_string("J", msgbuf.para[11], MSG);
			is_string("K", msgbuf.para[12], MSG);
			is_string(MKTEXT(471), msgbuf.para[13], MSG);
			is_string("L", msgbuf.para[14], MSG);
		}
	}
}

static void no_para_0_spaces(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value");

	is_int(1, msgbuf_parse(&msgbuf, tmp), MSG);
}

static void no_para_1_space(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value ");

	is_int(2, msgbuf_parse(&msgbuf, tmp), MSG);
}

static void no_para_2_spaces(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value  ");

	is_int(3, msgbuf_parse(&msgbuf, tmp), MSG);
}

static void no_para_3_spaces(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value   ");

	is_int(3, msgbuf_parse(&msgbuf, tmp), MSG);
}

static void origin_with_para1(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value :origin. PRIVMSG");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("origin.", msgbuf.origin, MSG);

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(1, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
		}
	}
}

static void origin_with_para3(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value :origin. PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("value", msgbuf.tags[0].value, MSG);
		}

		is_string("origin.", msgbuf.origin, MSG);

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void origin_no_para_0_spaces(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value :origin");

	is_int(4, msgbuf_parse(&msgbuf, tmp), MSG);
}

static void origin_no_para_1_space(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value :origin ");

	is_int(2, msgbuf_parse(&msgbuf, tmp), MSG);
}

static void origin_no_para_2_spaces(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value :origin  ");

	is_int(3, msgbuf_parse(&msgbuf, tmp), MSG);
}

static void origin_no_para_3_spaces(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=value :origin   ");

	is_int(3, msgbuf_parse(&msgbuf, tmp), MSG);
}

static void unescape_test(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag1=:\\:\\s\\\\\\r\\n;tag2=^:^\\:^\\s^\\\\^\\r^\\n^;tag3=\\:;tag4=\\\\;tag5=\\s PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(5, msgbuf.n_tags, MSG)) {
			is_string("tag1", msgbuf.tags[0].key, MSG);
			is_string(":; \\\r\n", msgbuf.tags[0].value, MSG);

			is_string("tag2", msgbuf.tags[1].key, MSG);
			is_string("^:^;^ ^\\^\r^\n^", msgbuf.tags[1].value, MSG);

			is_string("tag3", msgbuf.tags[2].key, MSG);
			is_string(";", msgbuf.tags[2].value, MSG);

			is_string("tag4", msgbuf.tags[3].key, MSG);
			is_string("\\", msgbuf.tags[3].value, MSG);

			is_string("tag5", msgbuf.tags[4].key, MSG);
			is_string(" ", msgbuf.tags[4].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void unescape_bad_test1(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag1=\\ PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag1", msgbuf.tags[0].key, MSG);
			is_string("", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void unescape_bad_test2(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag1=\\; PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag1", msgbuf.tags[0].key, MSG);
			is_string("", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void unescape_bad_test3(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag1=\\;tag2=value2 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(2, msgbuf.n_tags, MSG)) {
			is_string("tag1", msgbuf.tags[0].key, MSG);
			is_string("", msgbuf.tags[0].value, MSG);

			is_string("tag2", msgbuf.tags[1].key, MSG);
			is_string("value2", msgbuf.tags[1].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void unescape_bad_test4(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag1\\=value1;ta\\g2=val\\=ue2 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(2, msgbuf.n_tags, MSG)) {
			is_string("tag1\\", msgbuf.tags[0].key, MSG);
			is_string("value1", msgbuf.tags[0].value, MSG);

			is_string("ta\\g2", msgbuf.tags[1].key, MSG);
			is_string("val=ue2", msgbuf.tags[1].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void unescape_bad_test5(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag1=\\v\\a\\l\\u\\e\\1;tag2=\\va\\lu\\e2;tag3=v\\al\\ue\\3 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(3, msgbuf.n_tags, MSG)) {
			is_string("tag1", msgbuf.tags[0].key, MSG);
			is_string("value1", msgbuf.tags[0].value, MSG);

			is_string("tag2", msgbuf.tags[1].key, MSG);
			is_string("value2", msgbuf.tags[1].value, MSG);

			is_string("tag3", msgbuf.tags[2].key, MSG);
			is_string("value3", msgbuf.tags[2].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void unescape_bad_test6(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@\\=value1;tag2=value2 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(2, msgbuf.n_tags, MSG)) {
			is_string("\\", msgbuf.tags[0].key, MSG);
			is_string("value1", msgbuf.tags[0].value, MSG);

			is_string("tag2", msgbuf.tags[1].key, MSG);
			is_string("value2", msgbuf.tags[1].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static void unescape_8bit(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "@tag=\176\177\178\376\377 PRIVMSG #test :test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		if (is_int(1, msgbuf.n_tags, MSG)) {
			is_string("tag", msgbuf.tags[0].key, MSG);
			is_string("\176\177\178\376\377", msgbuf.tags[0].value, MSG);
		}

		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string("test", msgbuf.para[2], MSG);
		}
	}
}

static struct MsgBuf *reconstruct_tail_prep(char *line, size_t n)
{
	static struct MsgBuf msgbuf;
	msgbuf_init(&msgbuf);
	msgbuf_parse(&msgbuf, line);
	msgbuf_reconstruct_tail(&msgbuf, n);
	return &msgbuf;
}

static void reconstruct_tail(void)
{
	struct MsgBuf *mb;
	mb = reconstruct_tail_prep((char[]){"CMD P1"}, 2);
	is_string("CMD", mb->para[0], MSG);
	is_string("P1", mb->para[1], MSG);

	mb = reconstruct_tail_prep((char[]){"CMD P1 P2"}, 2);
	is_string("CMD", mb->para[0], MSG);
	is_string("P1", mb->para[1], MSG);
	is_string("P2", mb->para[2], MSG);

	mb = reconstruct_tail_prep((char[]){"   CMD P1 P2 :P3"}, 0);
	is_string("CMD P1 P2 :P3", mb->para[0], MSG);

	mb = reconstruct_tail_prep((char[]){"CMD  P1 P2 :P3"}, 1);
	is_string(" P1 P2 :P3", mb->para[1], MSG);

	mb = reconstruct_tail_prep((char[]){"CMD P1   P2"}, 1);
	is_string("P1   P2", mb->para[1], MSG);

	mb = reconstruct_tail_prep((char[]){"CMD P1  P2 "}, 2);
	is_string(" P2 ", mb->para[2], MSG);

	mb = reconstruct_tail_prep((char[]){"CMD P1    "}, 2);
	is_string("   ", mb->para[2], MSG);

	mb = reconstruct_tail_prep((char[]){"CMD P1 :"}, 2);
	is_string(":", mb->para[2], MSG);

	mb = reconstruct_tail_prep((char[]){"CMD P1  :"}, 2);
	is_string(" :", mb->para[2], MSG);
}

static void basic_trailing_with_colon(void)
{
	struct MsgBuf msgbuf;

	memset(&msgbuf, 0, sizeof(msgbuf));
	strcpy(tmp, "PRIVMSG #test ::test test");

	if (is_int(0, msgbuf_parse(&msgbuf, tmp), MSG)) {
		is_string("PRIVMSG", msgbuf.cmd, MSG);
		if (is_int(3, msgbuf.n_para, MSG)) {
			is_string("PRIVMSG", msgbuf.para[0], MSG);
			is_string("#test", msgbuf.para[1], MSG);
			is_string(":test test", msgbuf.para[2], MSG);
		}
	}
}

int main(int argc, char *argv[])
{
	memset(&me, 0, sizeof(me));
	strcpy(me.name, "me.name.");

	plan_lazy();

	is_int(8191, TAGSLEN, MSG);
	is_int(510, DATALEN, MSG);

	basic_tags1();
	basic_tags2();
	basic_tags3();
	basic_tags4();
	basic_tags5a();
	basic_tags5b();
	basic_tags13();
	basic_tags14();
	basic_tags15();
	basic_tags16();
	basic_tags30();
	basic_tags31();
	basic_tags32();

	empty_value1();
	empty_value2();

	malformed1a();
	malformed2a();
	malformed1b();
	malformed2b();
	malformed3();
	malformed4();
	malformed5();
	malformed6();
	malformed7();

	long_tag1();
	long_tag2();

	long_value1();

	too_long_tag1();
	too_long_tag2();
	too_long_tag3();
	too_long_tags30a();
	too_long_tags30b();
	too_long_tags31a();
	too_long_tags31b();
	too_long_tags32a();
	too_long_tags32b();

	too_long_value1();
	too_long_value2();
	too_long_value3();

	basic_para3a();
	basic_para3b();
	basic_para13();
	basic_para14a();
	basic_para14b();
	basic_para14c();
	basic_para15a();
	basic_para15b();
	basic_para15c();
	basic_para15d();
	basic_para16a();
	basic_para16b();
	basic_para16c();
	basic_para17a();
	basic_para17b();
	basic_para17c();
	basic_para17d();
	basic_trailing_with_colon();

	long_para1();
	long_para2();
	long_para3();
	long_para4();

	too_long_para1();
	too_long_para2();

	long_everything1();
	long_everything2();
	long_everything3a();
	long_everything3b();
	long_everything4();

	too_long_everything1();
	too_long_everything2();
	too_long_everything3a();
	too_long_everything3b();
	too_long_everything4();

	no_para_0_spaces();
	no_para_1_space();
	no_para_2_spaces();
	no_para_3_spaces();

	origin_with_para1();
	origin_with_para3();
	origin_no_para_0_spaces();
	origin_no_para_1_space();
	origin_no_para_2_spaces();
	origin_no_para_3_spaces();

	unescape_test();
	unescape_bad_test1();
	unescape_bad_test2();
	unescape_bad_test3();
	unescape_bad_test4();
	unescape_bad_test5();
	unescape_bad_test6();

	unescape_8bit();

	reconstruct_tail();

	return 0;
}
