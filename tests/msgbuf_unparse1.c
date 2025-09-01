/*
 *  msgbuf_unparse1.c: Test msgbuf under various conditions
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
;

#define MKTEXT(n) &text[sizeof(text) - ((n) + 1)]

/* must be much larger than the intended maximum output tags/data size */
#define OUTPUT_BUFSIZE (EXT_BUFSIZE * 4)

static void basic_tags1(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 1,
		.tags = {
			{ .key = "tag", .value = "value", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@tag=value :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void basic_tags2(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "tag1", .value = "value1", .capmask = 1 },
			{ .key = "tag2", .value = "value2", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@tag1=value1;tag2=value2 :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void basic_tags30(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 30,
		.tags = {
			{ .key = "tag1", .value = "value1", .capmask = 1 },
			{ .key = "tag2", .value = "value2", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "tag4", .value = "value4", .capmask = 1 },
			{ .key = "tag5", .value = "value5", .capmask = 1 },
			{ .key = "tag6", .value = "value6", .capmask = 1 },
			{ .key = "tag7", .value = "value7", .capmask = 1 },
			{ .key = "tag8", .value = "value8", .capmask = 1 },
			{ .key = "tag9", .value = "value9", .capmask = 1 },
			{ .key = "tag10", .value = "value10", .capmask = 1 },
			{ .key = "tag11", .value = "value11", .capmask = 1 },
			{ .key = "tag12", .value = "value12", .capmask = 1 },
			{ .key = "tag13", .value = "value13", .capmask = 1 },
			{ .key = "tag14", .value = "value14", .capmask = 1 },
			{ .key = "tag15", .value = "value15", .capmask = 1 },
			{ .key = "tag16", .value = "value16", .capmask = 1 },
			{ .key = "tag17", .value = "value17", .capmask = 1 },
			{ .key = "tag18", .value = "value18", .capmask = 1 },
			{ .key = "tag19", .value = "value19", .capmask = 1 },
			{ .key = "tag20", .value = "value20", .capmask = 1 },
			{ .key = "tag21", .value = "value21", .capmask = 1 },
			{ .key = "tag22", .value = "value22", .capmask = 1 },
			{ .key = "tag23", .value = "value23", .capmask = 1 },
			{ .key = "tag24", .value = "value24", .capmask = 1 },
			{ .key = "tag25", .value = "value25", .capmask = 1 },
			{ .key = "tag26", .value = "value26", .capmask = 1 },
			{ .key = "tag27", .value = "value27", .capmask = 1 },
			{ .key = "tag28", .value = "value28", .capmask = 1 },
			{ .key = "tag29", .value = "value29", .capmask = 1 },
			{ .key = "tag30", .value = "value30", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag13=value13;tag14=value14;tag15=value15;tag16=value16;tag17=value17;tag18=value18;tag19=value19;tag20=value20;tag21=value21;tag22=value22;tag23=value23;tag24=value24;tag25=value25;tag26=value26;tag27=value27;tag28=value28;tag29=value29;tag30=value30 :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void basic_tags15_capmask1(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 15,
		.tags = {
			{ .key = "tag1", .value = "value1", .capmask = 0x1 },
			{ .key = "tag2", .value = "value2", .capmask = 0x3 },
			{ .key = "tag3", .value = "value3", .capmask = 0xffffffff },
			{ .key = "tag4", .value = "value4", .capmask = 0xffff },
			{ .key = "tag5", .value = "value5", .capmask = 0x0 },
			{ .key = "tag6", .value = "value6", .capmask = 0x3e9 },
			{ .key = "tag7", .value = "value7", .capmask = 0x2 },
			{ .key = "tag8", .value = "value8", .capmask = 0x65 },
			{ .key = "tag9", .value = "value9", .capmask = 0x5 },
			{ .key = "tag10", .value = "value10", .capmask = 0x7fffffff },
			{ .key = "tag11", .value = "value11", .capmask = 0x80000001 },
			{ .key = "tag12", .value = "value12", .capmask = 0xf },
			{ .key = "tag13", .value = "value13", .capmask = 0x6 },
			{ .key = "tag14", .value = "value14", .capmask = 0x4 },
			{ .key = "tag15", .value = "value15", .capmask = 0x7 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag6=value6;tag8=value8;tag9=value9;tag10=value10;tag11=value11;tag12=value12;tag15=value15 :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void basic_tags15_capmask2(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 15,
		.tags = {
			{ .key = "tag1", .value = "value1", .capmask = 0x1 },
			{ .key = "tag2", .value = "value2", .capmask = 0x3 },
			{ .key = "tag3", .value = "value3", .capmask = 0xffffffff },
			{ .key = "tag4", .value = "value4", .capmask = 0xffff },
			{ .key = "tag5", .value = "value5", .capmask = 0x0 },
			{ .key = "tag6", .value = "value6", .capmask = 0x3e9 },
			{ .key = "tag7", .value = "value7", .capmask = 0x2 },
			{ .key = "tag8", .value = "value8", .capmask = 0x65 },
			{ .key = "tag9", .value = "value9", .capmask = 0x5 },
			{ .key = "tag10", .value = "value10", .capmask = 0x7fffffff },
			{ .key = "tag11", .value = "value11", .capmask = 0x80000001 },
			{ .key = "tag12", .value = "value12", .capmask = 0xf },
			{ .key = "tag13", .value = "value13", .capmask = 0x6 },
			{ .key = "tag14", .value = "value14", .capmask = 0x4 },
			{ .key = "tag15", .value = "value15", .capmask = 0x7 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 0x24), MSG)) {
		is_string("@tag3=value3;tag4=value4;tag6=value6;tag8=value8;tag9=value9;tag10=value10;tag12=value12;tag13=value13;tag14=value14;tag15=value15 :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void no_tags(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,
		.tags = {},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string(":origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void empty_tag(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 1,
		.tags = {
			{ .key = "", .value = "value", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string(":origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void too_long_for_tag2a_0remaining(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2047), .capmask = 1 },
			{ .key = "tag2", .value = "value2", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4088), .capmask = 1 },
			{ .key = "+tag5", .value = "value5", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2047));
		strcat(tmp, ";+tag4=");
		strcat(tmp, MKTEXT(4088));
		strcat(tmp, " ");
		is_int(8191, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void too_long_for_tag2a_1remaining(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2046), .capmask = 1 },
			{ .key = "tag2", .value = "value2", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4087), .capmask = 1 },
			{ .key = "+tag5", .value = "value5", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, ";+tag4=");
		strcat(tmp, MKTEXT(4087));
		strcat(tmp, " ");
		is_int(8191 - 2, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void too_long_for_tag2a_2remaining(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2045), .capmask = 1 },
			{ .key = "tag2", .value = "value2", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4086), .capmask = 1 },
			{ .key = "+tag5", .value = "value5", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2045));
		strcat(tmp, ";+tag4=");
		strcat(tmp, MKTEXT(4086));
		strcat(tmp, " ");
		is_int(8191 - 4, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void too_long_for_tag2a_3remaining(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2044), .capmask = 1 },
			{ .key = "tag2", .value = "value2", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4085), .capmask = 1 },
			{ .key = "+tag5", .value = "value5", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2044));
		strcat(tmp, ";+tag4=");
		strcat(tmp, MKTEXT(4085));
		strcat(tmp, " ");
		is_int(8191 - 6, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void too_long_for_tag2a_3short(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2038), .capmask = 1 },
			{ .key = "tag2", .value = "value2", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4078), .capmask = 1 },
			{ .key = "+tag5", .value = "value5", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2038));
		strcat(tmp, ";+tag4=");
		strcat(tmp, MKTEXT(4078));
		strcat(tmp, " ");
		is_int(8191 - 19, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void too_long_for_tag2a_2short(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2037), .capmask = 1 },
			{ .key = "tag2", .value = "value2", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4077), .capmask = 1 },
			{ .key = "+tag5", .value = "value5", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2037));
		strcat(tmp, ";+tag4=");
		strcat(tmp, MKTEXT(4077));
		strcat(tmp, " ");
		is_int(8191 - 21, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void too_long_for_tag2a_1short(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2036), .capmask = 1 },
			{ .key = "tag2", .value = "value2", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4076), .capmask = 1 },
			{ .key = "+tag5", .value = "value5", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2036));
		strcat(tmp, ";+tag4=");
		strcat(tmp, MKTEXT(4076));
		strcat(tmp, " ");
		is_int(8191 - 23, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void exact_space_for_tag2a(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2035), .capmask = 1 },
			{ .key = "tag2", .value = "value2", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4075), .capmask = 1 },
			{ .key = "+tag5", .value = "value5", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2035));
		strcat(tmp, ";tag2=value2;+tag4=");
		strcat(tmp, MKTEXT(4075));
		strcat(tmp, ";+tag5=value5 ");
		is_int(8191, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void too_long_for_tag2b_0remaining(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2047), .capmask = 1 },
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4088), .capmask = 1 },
			{ .key = "+a", .value = "b", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2047));
		strcat(tmp, ";+tag4=");
		strcat(tmp, MKTEXT(4088));
		strcat(tmp, " ");
		is_int(8191, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void too_long_for_tag2b_1remaining(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2046), .capmask = 1 },
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4087), .capmask = 1 },
			{ .key = "+a", .value = "b", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, ";+tag4=");
		strcat(tmp, MKTEXT(4087));
		strcat(tmp, " ");
		is_int(8191 - 2, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void too_long_for_tag2b_2remaining(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2045), .capmask = 1 },
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4086), .capmask = 1 },
			{ .key = "+a", .value = "b", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2045));
		strcat(tmp, ";+tag4=");
		strcat(tmp, MKTEXT(4086));
		strcat(tmp, " ");
		is_int(8191 - 4, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void too_long_for_tag2b_3remaining(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2044), .capmask = 1 },
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4085), .capmask = 1 },
			{ .key = "+a", .value = "b", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2044));
		strcat(tmp, ";+tag4=");
		strcat(tmp, MKTEXT(4085));
		strcat(tmp, " ");
		is_int(8191 - 6, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void too_long_for_tag2b_3short(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2046), .capmask = 1 },
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4086), .capmask = 1 },
			{ .key = "+a", .value = "b", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, ";+tag4=");
		strcat(tmp, MKTEXT(4086));
		strcat(tmp, " ");
		is_int(8191 - 3, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void too_long_for_tag2b_2short(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2045), .capmask = 1 },
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4085), .capmask = 1 },
			{ .key = "+a", .value = "b", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2045));
		strcat(tmp, ";+tag4=");
		strcat(tmp, MKTEXT(4085));
		strcat(tmp, " ");
		is_int(8191 - 5, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void too_long_for_tag2b_1short(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2044), .capmask = 1 },
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4084), .capmask = 1 },
			{ .key = "+a", .value = "b", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2044));
		strcat(tmp, ";+tag4=");
		strcat(tmp, MKTEXT(4084));
		strcat(tmp, " ");
		is_int(8191 - 7, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void exact_space_for_tag2b(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = MKTEXT(2046), .value = MKTEXT(2043), .capmask = 1 },
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = "tag3", .value = "value3", .capmask = 1 },
			{ .key = "+tag4", .value = MKTEXT(4083), .capmask = 1 },
			{ .key = "+a", .value = "b", .capmask = 1 },
			{ .key = "+tag6", .value = "value6", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8300];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(2046));
		strcat(tmp, "=");
		strcat(tmp, MKTEXT(2043));
		strcat(tmp, ";a=b;+tag4=");
		strcat(tmp, MKTEXT(4083));
		strcat(tmp, ";+a=b ");
		is_int(8191, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void long_tag1_no_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 1,
		.tags = {
			{ .key = MKTEXT(4094), .value = NULL, .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(4094));
		strcat(tmp, " ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void long_tag1_empty_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 1,
		.tags = {
			{ .key = MKTEXT(4093), .value = "", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(4093));
		strcat(tmp, "= ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void long_tag1_short_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 1,
		.tags = {
			{ .key = MKTEXT(4092), .value = "x", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@");
		strcat(tmp, MKTEXT(4092));
		strcat(tmp, "=x ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void short_tag1_long_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 1,
		.tags = {
			{ .key = "x", .value = MKTEXT(4092), .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@x=");
		strcat(tmp, MKTEXT(4092));
		strcat(tmp, " ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void too_long_tag1_no_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 1,
		.tags = {
			{ .key = MKTEXT(4095), .value = NULL, .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string(":origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void too_long_tag1_empty_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 1,
		.tags = {
			{ .key = MKTEXT(4094), .value = "", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string(":origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void too_long_tag1_short_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 1,
		.tags = {
			{ .key = MKTEXT(4093), .value = "x", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string(":origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void short_tag1_too_long_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 1,
		.tags = {
			{ .key = "x", .value = MKTEXT(4093), .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string(":origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void long_tag2a_no_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = NULL, .capmask = 1 },
			{ .key = MKTEXT(4092), .value = NULL, .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@a;");
		strcat(tmp, MKTEXT(4092));
		strcat(tmp, " ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void long_tag2b_no_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "", .capmask = 1 },
			{ .key = MKTEXT(4091), .value = NULL, .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@a=;");
		strcat(tmp, MKTEXT(4091));
		strcat(tmp, " ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void long_tag2c_no_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = MKTEXT(4090), .value = NULL, .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@a=b;");
		strcat(tmp, MKTEXT(4090));
		strcat(tmp, " ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void long_tag2a_empty_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = NULL, .capmask = 1 },
			{ .key = MKTEXT(4091), .value = "", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@a;");
		strcat(tmp, MKTEXT(4091));
		strcat(tmp, "= ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void long_tag2b_empty_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "", .capmask = 1 },
			{ .key = MKTEXT(4090), .value = "", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@a=;");
		strcat(tmp, MKTEXT(4090));
		strcat(tmp, "= ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void long_tag2c_empty_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = MKTEXT(4089), .value = "", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@a=b;");
		strcat(tmp, MKTEXT(4089));
		strcat(tmp, "= ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void long_tag2a_short_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = NULL, .capmask = 1 },
			{ .key = MKTEXT(4090), .value = "x", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@a;");
		strcat(tmp, MKTEXT(4090));
		strcat(tmp, "=x ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void long_tag2b_short_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "", .capmask = 1 },
			{ .key = MKTEXT(4089), .value = "x", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@a=;");
		strcat(tmp, MKTEXT(4089));
		strcat(tmp, "=x ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void long_tag2c_short_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = MKTEXT(4088), .value = "x", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@a=b;");
		strcat(tmp, MKTEXT(4088));
		strcat(tmp, "=x ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void short_tag2a_long_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = NULL, .capmask = 1 },
			{ .key = "x", .value = MKTEXT(4090), .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@a;x=");
		strcat(tmp, MKTEXT(4090));
		strcat(tmp, " ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void short_tag2b_long_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "", .capmask = 1 },
			{ .key = "x", .value = MKTEXT(4089), .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@a=;x=");
		strcat(tmp, MKTEXT(4089));
		strcat(tmp, " ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void short_tag2c_long_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = "x", .value = MKTEXT(4088), .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[8192];

		strcpy(tmp, "@a=b;x=");
		strcat(tmp, MKTEXT(4088));
		strcat(tmp, " ");
		is_int(4096, strlen(tmp), MSG);
		strcat(tmp, ":origin PRIVMSG #test test test :test test");

		is_string(tmp, output, MSG);
	}
}

static void too_long_tag2a_no_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = NULL, .capmask = 1 },
			{ .key = MKTEXT(4093), .value = NULL, .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@a :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void too_long_tag2b_no_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "", .capmask = 1 },
			{ .key = MKTEXT(4092), .value = NULL, .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@a= :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void too_long_tag2c_no_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = MKTEXT(4091), .value = NULL, .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@a=b :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void too_long_tag2a_empty_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = NULL, .capmask = 1 },
			{ .key = MKTEXT(4092), .value = "", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@a :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void too_long_tag2b_empty_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "", .capmask = 1 },
			{ .key = MKTEXT(4091), .value = "", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@a= :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void too_long_tag2c_empty_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = MKTEXT(4090), .value = "", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@a=b :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void too_long_tag2a_short_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = NULL, .capmask = 1 },
			{ .key = MKTEXT(4092), .value = "x", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@a :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void too_long_tag2b_short_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "", .capmask = 1 },
			{ .key = MKTEXT(4091), .value = "x", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@a= :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void too_long_tag2c_short_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = MKTEXT(4090), .value = "x", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@a=b :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void short_tag2a_too_long_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = NULL, .capmask = 1 },
			{ .key = "x", .value = MKTEXT(4092), .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@a :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void short_tag2b_too_long_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "", .capmask = 1 },
			{ .key = "x", .value = MKTEXT(4091), .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@a= :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void short_tag2c_too_long_value(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 2,
		.tags = {
			{ .key = "a", .value = "b", .capmask = 1 },
			{ .key = "x", .value = MKTEXT(4090), .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@a=b :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void escape_test(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 6,
		.tags = {
			{ .key = "tag1", .value = ":; \\\r\n", .capmask = 1 },
			{ .key = "tag2", .value = "^:^;^ ^\\^\r^\n^", .capmask = 1 },
			{ .key = "tag3", .value = ":", .capmask = 1 },
			{ .key = "tag4", .value = ";", .capmask = 1 },
			{ .key = "tag5", .value = "\\", .capmask = 1 },
			{ .key = "tag6", .value = " ", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@tag1=:\\:\\s\\\\\\r\\n;tag2=^:^\\:^\\s^\\\\^\\r^\\n^;tag3=:;tag4=\\:;tag5=\\\\;tag6=\\s :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void escape_test_8bit(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 1,
		.tags = {
			{ .key = "tag1", .value = "\176\177\178\376\377", .capmask = 1 },
		},

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test", "test", "test test" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string("@tag1=\176\177\178\376\377 :origin PRIVMSG #test test test :test test", output, MSG);
	}
}

static void long_para1a(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 1,
		.para = { MKTEXT(488) },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test ");
		strcat(tmp, MKTEXT(488));
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static void too_long_para1a(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 1,
		.para = { MKTEXT(489) },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test ");
		strcat(tmp, MKTEXT(489));
		tmp[strlen(tmp) - 1] = 0;
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static void long_para1b(void)
{
	char input[2048];
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 1,
		.para = { input },
	};
	char output[OUTPUT_BUFSIZE];

	/* make second-last char ' ' so that a ':' prefix is required */
	strcpy(input, MKTEXT(487));
	input[strlen(input) - 2] = ' ';

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test :");
		strcat(tmp, MKTEXT(487));
		tmp[strlen(tmp) - 2] = ' ';
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static void too_long_para1b(void)
{
	char input[2048];
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 1,
		.para = { input },
	};
	char output[OUTPUT_BUFSIZE];

	/* make second-last char ' ' so that a ':' prefix is required */
	strcpy(input, MKTEXT(488));
	input[strlen(input) - 3] = ' ';

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test :");
		strcat(tmp, MKTEXT(488));
		tmp[strlen(tmp) - 3] = ' ';
		tmp[strlen(tmp) - 1] = 0;
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static void long_para2a(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 2,
		.para = { "test", MKTEXT(483) },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test test ");
		strcat(tmp, MKTEXT(483));
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static void too_long_para2a(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 2,
		.para = { "test", MKTEXT(484) },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test test ");
		strcat(tmp, MKTEXT(484));
		tmp[strlen(tmp) - 1] = 0;
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static void long_para2b(void)
{
	char input[2048];
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 2,
		.para = { "test", input },
	};
	char output[OUTPUT_BUFSIZE];

	/* make second-last char ' ' so that a ':' prefix is required */
	strcpy(input, MKTEXT(482));
	input[strlen(input) - 2] = ' ';

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test test :");
		strcat(tmp, MKTEXT(482));
		tmp[strlen(tmp) - 2] = ' ';
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static void too_long_para2b(void)
{
	char input[2048];
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 2,
		.para = { "test", input },
	};
	char output[OUTPUT_BUFSIZE];

	/* make second-last char ' ' so that a ':' prefix is required */
	strcpy(input, MKTEXT(483));
	input[strlen(input) - 3] = ' ';

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test test :");
		strcat(tmp, MKTEXT(483));
		tmp[strlen(tmp) - 3] = ' ';
		tmp[strlen(tmp) - 1] = 0;
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static void long_para3a(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test1", "test2", MKTEXT(476) },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test test1 test2 ");
		strcat(tmp, MKTEXT(476));
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static void too_long_para3a(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test1", "test2", MKTEXT(477) },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test test1 test2 ");
		strcat(tmp, MKTEXT(477));
		tmp[strlen(tmp) - 1] = 0;
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static void long_para3b(void)
{
	char input[2048];
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test1", "test2", input },
	};
	char output[OUTPUT_BUFSIZE];

	/* make second-last char ' ' so that a ':' prefix is required */
	strcpy(input, MKTEXT(475));
	input[strlen(input) - 2] = ' ';

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test test1 test2 :");
		strcat(tmp, MKTEXT(475));
		tmp[strlen(tmp) - 2] = ' ';
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static void too_long_para3b(void)
{
	char input[2048];
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test1", "test2", input },
	};
	char output[OUTPUT_BUFSIZE];

	/* make second-last char ' ' so that a ':' prefix is required */
	strcpy(input, MKTEXT(476));
	input[strlen(input) - 3] = ' ';

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test test1 test2 :");
		strcat(tmp, MKTEXT(476));
		tmp[strlen(tmp) - 3] = ' ';
		tmp[strlen(tmp) - 1] = 0;
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static void long_para15a(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 15,
		.para = { "test1", "test2", "test3", "test4", "test5", "test6", "test7", "test8", "test9", "test10", "test11", "test12", "test13", "test14", MKTEXT(399) },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test11 test12 test13 test14 ");
		strcat(tmp, MKTEXT(399));
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static void too_long_para15a(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 15,
		.para = { "test1", "test2", "test3", "test4", "test5", "test6", "test7", "test8", "test9", "test10", "test11", "test12", "test13", "test14", MKTEXT(400) },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test11 test12 test13 test14 ");
		strcat(tmp, MKTEXT(400));
		tmp[strlen(tmp) - 1] = 0;
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static void long_para15b(void)
{
	char input[2048];
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 15,
		.para = { "test1", "test2", "test3", "test4", "test5", "test6", "test7", "test8", "test9", "test10", "test11", "test12", "test13", "test14", input },
	};
	char output[OUTPUT_BUFSIZE];

	/* make second-last char ' ' so that a ':' prefix is required */
	strcpy(input, MKTEXT(398));
	input[strlen(input) - 2] = ' ';

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test11 test12 test13 test14 :");
		strcat(tmp, MKTEXT(398));
		tmp[strlen(tmp) - 2] = ' ';
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static void too_long_para15b(void)
{
	char input[2048];
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = "#test",

		.n_para = 15,
		.para = { "test1", "test2", "test3", "test4", "test5", "test6", "test7", "test8", "test9", "test10", "test11", "test12", "test13", "test14", input },
	};
	char output[OUTPUT_BUFSIZE];

	/* make second-last char ' ' so that a ':' prefix is required */
	strcpy(input, MKTEXT(399));
	input[strlen(input) - 3] = ' ';

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		char tmp[2048];

		strcpy(tmp, ":origin PRIVMSG #test test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 test11 test12 test13 test14 :");
		strcat(tmp, MKTEXT(399));
		tmp[strlen(tmp) - 3] = ' ';
		tmp[strlen(tmp) - 1] = 0;
		is_int(510, strlen(tmp), MSG);

		is_string(tmp, output, MSG);
	}
}

static const struct MsgBuf small_buffer_tags_msgbuf = {
	.n_tags = 15,
	.tags = {
		{ .key = "tag1", .value = "value1", .capmask = 1 },
		{ .key = "tag2", .value = "value2", .capmask = 1 },
		{ .key = "tag3", .value = "value3", .capmask = 1 },
		{ .key = "tag4", .value = "value4", .capmask = 1 },
		{ .key = "tag5", .value = "value5", .capmask = 1 },
		{ .key = "tag6", .value = "value6", .capmask = 1 },
		{ .key = "tag7", .value = "value7", .capmask = 1 },
		{ .key = "tag8", .value = "value8", .capmask = 1 },
		{ .key = "tag9", .value = "value9", .capmask = 1 },
		{ .key = "tag10", .value = "value10", .capmask = 1 },
		{ .key = "tag11", .value = "value11", .capmask = 1 },
		{ .key = "tag12", .value = "value12", .capmask = 1 },
		{ .key = "tag13", .value = "value13", .capmask = 1 },
		{ .key = "tag14", .value = "value14", .capmask = 1 },
		{ .key = "tag15", .value = "value15", .capmask = 1 },
	},

	.cmd = "PRIVMSG",
	.origin = "origin",
	.target = "#test",

	.n_para = 3,
	.para = { "test", "test", "test test" },
};

static void small_buffer_tags_2_remaining_for_para(void)
{
	char output[100];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_tags_msgbuf, 1), MSG)) {
		is_string("@tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8 :o", output, MSG);
	}
}

static void small_buffer_tags_1_remaining_for_para(void)
{
	char output[99];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_tags_msgbuf, 1), MSG)) {
		is_string("@tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8 :", output, MSG);
	}
}

static void small_buffer_tags_0_remaining_for_para(void)
{
	char output[98];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_tags_msgbuf, 1), MSG)) {
		is_string("@tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7;tag8=value8 ", output, MSG);
	}
}

static void small_buffer_tags_X_remaining_for_para(void)
{
	char output[97];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_tags_msgbuf, 1), MSG)) {
		// Must leave enough space for a tag but not enough space for the tag
		//        "                                                                                    ;tag8=value8 "
		is_string("@tag1=value1;tag2=value2;tag3=value3;tag4=value4;tag5=value5;tag6=value6;tag7=value7 :origin PRI", output, MSG);
	}
}

static const struct MsgBuf small_buffer_a_msgbuf = {
	.n_tags = 3,
	.tags = {
		{ .key = "a", .value = NULL, .capmask = 1 },
		{ .key = "c", .value = "d", .capmask = 1 },
		{ .key = "e", .value = "f", .capmask = 1 },
	},

	.cmd = "PRIVMSG",
	.origin = "origin",
	.target = "#test",

	.n_para = 3,
	.para = { "test", "test", "test test" },
};

static const struct MsgBuf small_buffer_b_msgbuf = {
	.n_tags = 3,
	.tags = {
		{ .key = "a", .value = "b", .capmask = 1 },
		{ .key = "c", .value = "d", .capmask = 1 },
		{ .key = "e", .value = "f", .capmask = 1 },
	},

	.cmd = "PRIVMSG",
	.origin = "origin",
	.target = "#test",

	.n_para = 3,
	.para = { "test", "test", "test test" },
};

static void small_buffer1(void)
{
	char output[1];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_a_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("", output, MSG);
	}

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_b_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("", output, MSG);
	}
}

static void small_buffer2(void)
{
	char output[2];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_a_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string(":", output, MSG);
	}

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_b_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string(":", output, MSG);
	}
}

static void small_buffer3(void)
{
	char output[3];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_a_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string(":o", output, MSG);
	}

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_b_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string(":o", output, MSG);
	}
}

static void small_buffer4(void)
{
	char output[4];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_a_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a ", output, MSG);
	}

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_b_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string(":or", output, MSG);
	}
}

static void small_buffer5(void)
{
	char output[5];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_a_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a :", output, MSG);
	}

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_b_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string(":ori", output, MSG);
	}
}

static void small_buffer6(void)
{
	char output[6];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_a_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a :o", output, MSG);
	}

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_b_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a=b ", output, MSG);
	}
}

static void small_buffer7(void)
{
	char output[7];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_a_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a :or", output, MSG);
	}

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_b_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a=b :", output, MSG);
	}
}

static void small_buffer8(void)
{
	char output[8];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_a_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a;c=d ", output, MSG);
	}

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_b_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a=b :o", output, MSG);
	}
}

static void small_buffer9(void)
{
	char output[9];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_a_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a;c=d :", output, MSG);
	}

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_b_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a=b :or", output, MSG);
	}
}

static void small_buffer10(void)
{
	char output[10];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_a_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a;c=d :o", output, MSG);
	}

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_b_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a=b;c=d ", output, MSG);
	}
}

static void small_buffer11(void)
{
	char output[11];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_a_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a;c=d :or", output, MSG);
	}

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_b_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a=b;c=d :", output, MSG);
	}
}

static void small_buffer12(void)
{
	char output[12];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_a_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a;c=d;e=f ", output, MSG);
	}

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_b_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a=b;c=d :o", output, MSG);
	}
}

static void small_buffer13(void)
{
	char output[13];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_a_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a;c=d;e=f :", output, MSG);
	}

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_b_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a=b;c=d :or", output, MSG);
	}
}

static void small_buffer14(void)
{
	char output[14];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_a_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a;c=d;e=f :o", output, MSG);
	}

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_b_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a=b;c=d;e=f ", output, MSG);
	}
}

static void small_buffer15(void)
{
	char output[15];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_a_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a;c=d;e=f :or", output, MSG);
	}

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &small_buffer_b_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("@a=b;c=d;e=f :", output, MSG);
	}
}

static const struct MsgBuf long_tags_small_buffer_msgbuf = {
	.n_tags = 2,
	.tags = {
		{ .key = MKTEXT(4094), .value = NULL, .capmask = 1 },
		{ .key = "+a", .value = MKTEXT(4091), .capmask = 1 },
	},

	.cmd = "PRIVMSG",
	.origin = "origin",
	.target = "#test",

	.n_para = 3,
	.para = { "test", "test", "test test" },
};

static char long_tags_str[8192];

static void long_tags_init(void)
{
	strcpy(long_tags_str, "@");
	strcat(long_tags_str, MKTEXT(4094));
	strcat(long_tags_str, ";+a=");
	strcat(long_tags_str, MKTEXT(4091));
	strcat(long_tags_str, " ");
}

static void long_tags_small_buffer0(void)
{
	char output[TAGSLEN + 0 + 1];
	char tmp[8200];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &long_tags_small_buffer_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		strcpy(tmp, long_tags_str);
		is_string(tmp, output, MSG);
	}
}

static void long_tags_small_buffer1(void)
{
	char output[TAGSLEN + 1 + 1];
	char tmp[8200];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &long_tags_small_buffer_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		strcpy(tmp, long_tags_str);
		strcat(tmp, ":");
		is_string(tmp, output, MSG);
	}
}

static void long_tags_small_buffer2(void)
{
	char output[TAGSLEN + 2 + 1];
	char tmp[8200];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &long_tags_small_buffer_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		strcpy(tmp, long_tags_str);
		strcat(tmp, ":o");
		is_string(tmp, output, MSG);
	}
}

static void long_tags_small_buffer3(void)
{
	char output[TAGSLEN + 3 + 1];
	char tmp[8200];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &long_tags_small_buffer_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		strcpy(tmp, long_tags_str);
		strcat(tmp, ":or");
		is_string(tmp, output, MSG);
	}
}

static void long_tags_small_buffer4(void)
{
	char output[TAGSLEN + 4 + 1];
	char tmp[8200];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &long_tags_small_buffer_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		strcpy(tmp, long_tags_str);
		strcat(tmp, ":ori");
		is_string(tmp, output, MSG);
	}
}

static void long_tags_small_buffer5(void)
{
	char output[TAGSLEN + 5 + 1];
	char tmp[8200];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &long_tags_small_buffer_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		strcpy(tmp, long_tags_str);
		strcat(tmp, ":orig");
		is_string(tmp, output, MSG);
	}
}

static const struct MsgBuf no_tags_small_buffer_msgbuf = {
	.n_tags = 0,

	.cmd = "PRIVMSG",
	.origin = "origin",
	.target = "#test",

	.n_para = 3,
	.para = { "test", "test", "test test" },
};

static void no_tags_small_buffer1(void)
{
	char output[1];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &no_tags_small_buffer_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string("", output, MSG);
	}
}

static void no_tags_small_buffer2(void)
{
	char output[2];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &no_tags_small_buffer_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string(":", output, MSG);
	}
}

static void no_tags_small_buffer3(void)
{
	char output[3];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &no_tags_small_buffer_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string(":o", output, MSG);
	}
}

static void no_tags_small_buffer4(void)
{
	char output[4];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &no_tags_small_buffer_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string(":or", output, MSG);
	}
}

static void no_tags_small_buffer5(void)
{
	char output[5];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &no_tags_small_buffer_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string(":ori", output, MSG);
	}
}

static void no_tags_small_buffer6(void)
{
	char output[6];

	memset(output, 0, sizeof(output));
	if (is_int(0, msgbuf_unparse(output, sizeof(output), &no_tags_small_buffer_msgbuf, 1), MSG)) {
		is_int(sizeof(output) - 1, strlen(output), MSG);
		is_string(":orig", output, MSG);
	}
}

static void para_no_origin(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = NULL,
		.target = "#test",

		.n_para = 3,
		.para = { "test1", "test2", "test3 test4" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string(":me.name. PRIVMSG #test test1 test2 :test3 test4", output, MSG);
	}
}

static void para_no_cmd(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = NULL,
		.origin = "origin",
		.target = "#test",

		.n_para = 3,
		.para = { "test1", "test2", "test3 test4" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string(":origin #test test1 test2 :test3 test4", output, MSG);
	}
}

static void para_no_target(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = "origin",
		.target = NULL,

		.n_para = 3,
		.para = { "test1", "test2", "test3 test4" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string(":origin PRIVMSG test1 test2 :test3 test4", output, MSG);
	}
}

static void para_no_origin_no_cmd(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = NULL,
		.origin = NULL,
		.target = "#test",

		.n_para = 3,
		.para = { "test1", "test2", "test3 test4" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string(":me.name. #test test1 test2 :test3 test4", output, MSG);
	}
}

static void para_no_origin_no_target(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = "PRIVMSG",
		.origin = NULL,
		.target = NULL,

		.n_para = 3,
		.para = { "test1", "test2", "test3 test4" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string(":me.name. PRIVMSG test1 test2 :test3 test4", output, MSG);
	}
}

static void para_no_cmd_no_target(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = NULL,
		.origin = "origin",
		.target = NULL,

		.n_para = 3,
		.para = { "test1", "test2", "test3 test4" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string(":origin test1 test2 :test3 test4", output, MSG);
	}
}

static void para_no_origin_no_cmd_no_target(void)
{
	const struct MsgBuf msgbuf = {
		.n_tags = 0,

		.cmd = NULL,
		.origin = NULL,
		.target = NULL,

		.n_para = 3,
		.para = { "test1", "test2", "test3 test4" },
	};
	char output[OUTPUT_BUFSIZE];

	if (is_int(0, msgbuf_unparse(output, sizeof(output), &msgbuf, 1), MSG)) {
		is_string(":me.name. test1 test2 :test3 test4", output, MSG);
	}
}

int main(int argc, char *argv[])
{
	memset(&me, 0, sizeof(me));
	strcpy(me.name, "me.name.");

	plan_lazy();

	is_int(8191, TAGSLEN, MSG);
	is_int(510, DATALEN, MSG);
	is_int(8191 + 510 + 1, EXT_BUFSIZE, MSG);

	basic_tags1();
	basic_tags2();
	basic_tags30();
	basic_tags15_capmask1();
	basic_tags15_capmask2();

	no_tags();
	empty_tag();

	long_tag1_no_value();
	long_tag1_empty_value();
	long_tag1_short_value();
	short_tag1_long_value();
	short_tag1_long_value();
	short_tag1_long_value();

	too_long_tag1_no_value();
	too_long_tag1_empty_value();
	too_long_tag1_short_value();
	short_tag1_too_long_value();
	short_tag1_too_long_value();
	short_tag1_too_long_value();

	long_tag2a_no_value();
	long_tag2b_no_value();
	long_tag2c_no_value();
	long_tag2a_empty_value();
	long_tag2b_empty_value();
	long_tag2c_empty_value();
	long_tag2a_short_value();
	long_tag2b_short_value();
	long_tag2c_short_value();
	short_tag2a_long_value();
	short_tag2b_long_value();
	short_tag2c_long_value();

	too_long_tag2a_no_value();
	too_long_tag2b_no_value();
	too_long_tag2c_no_value();
	too_long_tag2a_empty_value();
	too_long_tag2b_empty_value();
	too_long_tag2c_empty_value();
	too_long_tag2a_short_value();
	too_long_tag2b_short_value();
	too_long_tag2c_short_value();
	short_tag2a_too_long_value();
	short_tag2b_too_long_value();
	short_tag2c_too_long_value();

	too_long_for_tag2a_0remaining();
	too_long_for_tag2a_1remaining();
	too_long_for_tag2a_2remaining();
	too_long_for_tag2a_3remaining();
	too_long_for_tag2a_3short();
	too_long_for_tag2a_2short();
	too_long_for_tag2a_1short();
	exact_space_for_tag2a();

	too_long_for_tag2b_0remaining();
	too_long_for_tag2b_1remaining();
	too_long_for_tag2b_2remaining();
	too_long_for_tag2b_3remaining();
	too_long_for_tag2b_3short();
	too_long_for_tag2b_2short();
	too_long_for_tag2b_1short();
	exact_space_for_tag2b();

	escape_test();
	escape_test_8bit();

	long_para1a();
	too_long_para1a();
	long_para1b();
	too_long_para1b();

	long_para2a();
	too_long_para2a();
	long_para2b();
	too_long_para2b();

	long_para3a();
	too_long_para3a();
	long_para3b();
	too_long_para3b();

	long_para15a();
	too_long_para15a();
	long_para15b();
	too_long_para15b();

	small_buffer_tags_2_remaining_for_para();
	small_buffer_tags_1_remaining_for_para();
	small_buffer_tags_0_remaining_for_para();
	small_buffer_tags_X_remaining_for_para();

	small_buffer1();
	small_buffer2();
	small_buffer3();
	small_buffer4();
	small_buffer5();
	small_buffer6();
	small_buffer7();
	small_buffer8();
	small_buffer9();
	small_buffer10();
	small_buffer11();
	small_buffer12();
	small_buffer13();
	small_buffer14();
	small_buffer15();

	long_tags_init();
	long_tags_small_buffer0();
	long_tags_small_buffer1();
	long_tags_small_buffer2();
	long_tags_small_buffer3();
	long_tags_small_buffer4();
	long_tags_small_buffer5();

	no_tags_small_buffer1();
	no_tags_small_buffer2();
	no_tags_small_buffer3();
	no_tags_small_buffer4();
	no_tags_small_buffer5();
	no_tags_small_buffer6();

	para_no_origin();
	para_no_cmd();
	para_no_target();
	para_no_origin_no_cmd();
	para_no_origin_no_target();
	para_no_cmd_no_target();
	para_no_origin_no_cmd_no_target();

	// TODO msgbuf_vunparse_fmt

	return 0;
}
