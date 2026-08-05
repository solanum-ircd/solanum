/*
 *  rb_snprintf_append1.c: Test rb_snprintf_append under various conditions
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
#include "client.h"
#include "rb_lib.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __func__

struct Client me;

static void from_empty1(void)
{
	char output[2048] = { 0 };
	int len;

	len = rb_snprintf_append(output, sizeof(output), "%s", "test");
	is_int(4, len, MSG);
	is_string("test", output, MSG);
}

static void basic_append1(void)
{
	char output[2048] = { 0 };
	int len;

	strcat(output, "test");

	len = rb_snprintf_append(output, sizeof(output), "%s", "TESTING");
	is_int(11, len, MSG);
	is_string("testTESTING", output, MSG);
}

static void append_empty1(void)
{
	char output[2048] = { 0 };
	int len;

	strcat(output, "test");

	len = rb_snprintf_append(output, sizeof(output), "%s", "");
	is_int(4, len, MSG);
	is_string("test", output, MSG);
}

static void exact_fit_empty1(void)
{
	char output[5] = { 0 };
	int len;

	strcat(output, "test");

	len = rb_snprintf_append(output, sizeof(output), "%s", "");
	is_int(4, len, MSG);
	is_string("test", output, MSG);
}

static void exact_fit_basic_append1(void)
{
	char output[12] = { 0 };
	int len;

	strcat(output, "test");

	len = rb_snprintf_append(output, sizeof(output), "%s", "TESTING");
	is_int(11, len, MSG);
	is_string("testTESTING", output, MSG);
}

static void too_long_basic_append1(void)
{
	char output[11] = { 0 };
	int len;

	strcat(output, "test");

	len = rb_snprintf_append(output, sizeof(output), "%s", "TESTING");
	is_int(11, len, MSG);
	is_string("testTESTIN", output, MSG);
}

static void exact_fit_from_empty1(void)
{
	char output[5] = { 0 };
	int len;

	len = rb_snprintf_append(output, sizeof(output), "%s", "test");
	is_int(4, len, MSG);
	is_string("test", output, MSG);
}

static void too_long_from_empty1(void)
{
	char output[4] = { 0 };
	int len;

	len = rb_snprintf_append(output, sizeof(output), "%s", "test");
	is_int(4, len, MSG);
	is_string("tes", output, MSG);
}

static void truncate_existing1(void)
{
	char output[2048] = { 0 };
	int len;

	strcat(output, "testing");

	len = rb_snprintf_append(output, 5, "%s", "");
	is_int(4, len, MSG);
	is_string("test", output, MSG);
}

static void truncate_existing2(void)
{
	char output[2048] = { 0 };
	int len;

	strcat(output, "testing");

	len = rb_snprintf_append(output, 5, "%s", "TEST");
	is_int(4, len, MSG);
	is_string("test", output, MSG);
}

static void no_buffer(void)
{
	int len;

	len = rb_snprintf_append(NULL, 0, "%s", "test");
	is_int(-1, len, MSG);
}

int main(int argc, char *argv[])
{
	memset(&me, 0, sizeof(me));
	strcpy(me.name, "me.name.");

	rb_lib_init(NULL, NULL, NULL, 0, 1024, DNODE_HEAP_SIZE, FD_HEAP_SIZE);
	rb_linebuf_init(LINEBUF_HEAP_SIZE);

	plan_lazy();

	from_empty1();
	basic_append1();
	append_empty1();

	exact_fit_from_empty1();
	too_long_from_empty1();

	exact_fit_basic_append1();
	too_long_basic_append1();

	exact_fit_empty1();

	truncate_existing1();
	truncate_existing2();
	no_buffer();

	return 0;
}
