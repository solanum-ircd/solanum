/*
 *  substition.c: Test substitution under various conditions
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
#include "substitution.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __func__

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
;

#define MKTEXT(n) &text[sizeof(text) - ((n) + 1)]

static rb_dlink_list varlist = { NULL, NULL, 0 };
static rb_dlink_list varlist2 = { NULL, NULL, 0 };

static void valid_variable1(void)
{
	is_string("TEST1", substitution_parse("${test1}", &varlist), MSG);
}

static void valid_variable2(void)
{
	is_string("TEST1TEST5", substitution_parse("${test1}${test5}", &varlist), MSG);
}

static void missing_variable1(void)
{
	is_string("", substitution_parse("${test6}", &varlist), MSG);
}

static void missing_variable2(void)
{
	is_string("", substitution_parse("${test6}${test7}", &varlist), MSG);
}

static void unterminated_variable1a(void)
{
	is_string("TEST2", substitution_parse("${test2", &varlist), MSG);
}

static void unterminated_variable1b(void)
{
	is_string("TEST2", substitution_parse("${test2\0${test8", &varlist), MSG);
}

static void unterminated_variable2a(void)
{
	is_string("TEST3TEST4", substitution_parse("${test3${test4", &varlist), MSG);
}

static void unterminated_variable2b(void)
{
	is_string("TEST3TEST4", substitution_parse("${test3${test4\0${test9", &varlist), MSG);
}

static void unterminated_missing_variable1(void)
{
	is_string("", substitution_parse("${test6", &varlist), MSG);
}

static void unterminated_missing_variable2(void)
{
	is_string("", substitution_parse("${test6${test7", &varlist), MSG);
}

static void empty_variable1(void)
{
	is_string("", substitution_parse("${}", &varlist), MSG);
}

static void empty_variable2(void)
{
	is_string("}", substitution_parse("${${}}", &varlist), MSG);
}

static void empty_unterminated_variable1(void)
{
	is_string("", substitution_parse("${", &varlist), MSG);
}

static void empty_unterminated_variable2(void)
{
	is_string("", substitution_parse("${${", &varlist), MSG);
}

static void long_variable1a(void) {
	is_int(512, BUFSIZE, MSG);
	is_string(MKTEXT(510), substitution_parse("${text510}", &varlist2), MSG);
}

static void long_variable1b(void) {
	is_int(512, BUFSIZE, MSG);
	is_string(MKTEXT(511), substitution_parse("${text511}", &varlist2), MSG);
}

static void too_long_variable1a(void) {
	char tmp2[2048];

	strcpy(tmp2, MKTEXT(512));
	tmp2[511] = 0; /* text will be truncated to 511 chars */

	is_int(512, BUFSIZE, MSG);
	is_string(tmp2, substitution_parse("${text512}", &varlist2), MSG);
}

static void too_long_variable1b(void) {
	char tmp2[2048];

	strcpy(tmp2, MKTEXT(513));
	tmp2[511] = 0; /* text will be truncated to 511 chars */

	is_int(512, BUFSIZE, MSG);
	is_string(tmp2, substitution_parse("${text513}", &varlist2), MSG);
}

static void too_long_variable1c(void) {
	char tmp2[2048];

	strcpy(tmp2, MKTEXT(600));
	tmp2[511] = 0; /* text will be truncated to 511 chars */

	is_int(512, BUFSIZE, MSG);
	is_string(tmp2, substitution_parse("${text600}", &varlist2), MSG);
}

static void long_variable2a(void) {
	char tmp2[2048];

	strcpy(tmp2, MKTEXT(500));
	strcat(tmp2, MKTEXT(10));

	is_int(512, BUFSIZE, MSG);
	is_string(tmp2, substitution_parse("${text500}${text10}", &varlist2), MSG);
}

static void long_variable2b(void) {
	char tmp2[2048];

	strcpy(tmp2, MKTEXT(500));
	strcat(tmp2, MKTEXT(11));

	is_int(512, BUFSIZE, MSG);
	is_string(tmp2, substitution_parse("${text500}${text11}", &varlist2), MSG);
}

static void too_long_variable2a(void) {
	char tmp2[2048];

	strcpy(tmp2, MKTEXT(500));
	strcat(tmp2, MKTEXT(12));
	tmp2[511] = 0; /* text will be truncated to 511 chars */

	is_int(512, BUFSIZE, MSG);
	is_string(tmp2, substitution_parse("${text500}${text12}", &varlist2), MSG);
}

static void too_long_variable2b(void) {
	char tmp2[2048];

	strcpy(tmp2, MKTEXT(500));
	strcat(tmp2, MKTEXT(13));
	tmp2[511] = 0; /* text will be truncated to 511 chars */

	is_int(512, BUFSIZE, MSG);
	is_string(tmp2, substitution_parse("${text500}${text13}", &varlist2), MSG);
}

static void too_long_variable2c(void) {
	char tmp2[2048];

	strcpy(tmp2, MKTEXT(600));
	strcat(tmp2, MKTEXT(10));
	tmp2[511] = 0; /* text will be truncated to 511 chars */

	is_int(512, BUFSIZE, MSG);
	is_string(tmp2, substitution_parse("${text600}${text10}", &varlist2), MSG);
}

static void long_variable3a(void) {
	char tmp2[2048];

	strcpy(tmp2, MKTEXT(100));
	strcat(tmp2, MKTEXT(400));
	strcat(tmp2, MKTEXT(10));

	is_int(512, BUFSIZE, MSG);
	is_string(tmp2, substitution_parse("${text100}${text400}${text10}", &varlist2), MSG);
}

static void long_variable3b(void) {
	char tmp2[2048];

	strcpy(tmp2, MKTEXT(100));
	strcat(tmp2, MKTEXT(400));
	strcat(tmp2, MKTEXT(11));

	is_int(512, BUFSIZE, MSG);
	is_string(tmp2, substitution_parse("${text100}${text400}${text11}", &varlist2), MSG);
}

static void too_long_variable3a(void) {
	char tmp2[2048];

	strcpy(tmp2, MKTEXT(100));
	strcat(tmp2, MKTEXT(400));
	strcat(tmp2, MKTEXT(12));
	tmp2[511] = 0; /* text will be truncated to 511 chars */

	is_int(512, BUFSIZE, MSG);
	is_string(tmp2, substitution_parse("${text100}${text400}${text12}", &varlist2), MSG);
}

static void too_long_variable3b(void) {
	char tmp2[2048];

	strcpy(tmp2, MKTEXT(100));
	strcat(tmp2, MKTEXT(400));
	strcat(tmp2, MKTEXT(13));
	tmp2[511] = 0; /* text will be truncated to 511 chars */

	is_int(512, BUFSIZE, MSG);
	is_string(tmp2, substitution_parse("${text100}${text400}${text13}", &varlist2), MSG);
}

static void too_long_variable3c(void) {
	char tmp2[2048];

	strcpy(tmp2, MKTEXT(200));
	strcat(tmp2, MKTEXT(400));
	strcat(tmp2, MKTEXT(100));
	tmp2[511] = 0; /* text will be truncated to 511 chars */

	is_int(512, BUFSIZE, MSG);
	is_string(tmp2, substitution_parse("${text200}${text400}${text100}", &varlist2), MSG);
}

static void long_variable_name1(void) {
	char input[2048];

	strcpy(input, "${");
	strcat(input, MKTEXT(1000));
	strcat(input, "}");

	is_string("test", substitution_parse(input, &varlist2), MSG);
}

int main(int argc, char *argv[])
{
	memset(&me, 0, sizeof(me));
	strcpy(me.name, "me.name.");

	rb_lib_init(NULL, NULL, NULL, 0, 1024, DNODE_HEAP_SIZE, FD_HEAP_SIZE);
	rb_linebuf_init(LINEBUF_HEAP_SIZE);

	substitution_append_var(&varlist, "testB", "TESTB");
	substitution_append_var(&varlist, "test1", "TEST1");
	substitution_append_var(&varlist, "test2", "TEST2");
	substitution_append_var(&varlist, "test3", "TEST3");
	substitution_append_var(&varlist, "test4", "TEST4");
	substitution_append_var(&varlist, "test5", "TEST5");
	substitution_append_var(&varlist, "test9", "TEST9");
	substitution_append_var(&varlist, "testE", "TESTE");

	substitution_append_var(&varlist2, "text10", MKTEXT(10));
	substitution_append_var(&varlist2, "text11", MKTEXT(11));
	substitution_append_var(&varlist2, "text12", MKTEXT(12));
	substitution_append_var(&varlist2, "text13", MKTEXT(13));
	substitution_append_var(&varlist2, "text14", MKTEXT(14));
	substitution_append_var(&varlist2, "text100", MKTEXT(100));
	substitution_append_var(&varlist2, "text200", MKTEXT(200));
	substitution_append_var(&varlist2, "text400", MKTEXT(400));
	substitution_append_var(&varlist2, "text500", MKTEXT(500));
	substitution_append_var(&varlist2, "text510", MKTEXT(510));
	substitution_append_var(&varlist2, "text511", MKTEXT(511));
	substitution_append_var(&varlist2, "text512", MKTEXT(512));
	substitution_append_var(&varlist2, "text513", MKTEXT(513));
	substitution_append_var(&varlist2, "text514", MKTEXT(514));
	substitution_append_var(&varlist2, "text600", MKTEXT(600));

	char temp[2048];
	strcpy(temp, MKTEXT(1000));
	temp[BUFSIZE - 1] = '\0';
	substitution_append_var(&varlist2, temp, "test");

	plan_lazy();

	valid_variable1();
	valid_variable2();
	missing_variable1();
	missing_variable2();
	unterminated_variable1a();
	unterminated_variable1b();
	unterminated_variable2a();
	unterminated_variable2b();
	unterminated_missing_variable1();
	unterminated_missing_variable2();
	empty_variable1();
	empty_variable2();
	empty_unterminated_variable1();
	empty_unterminated_variable2();

	long_variable1a();
	long_variable1b();
	long_variable2a();
	long_variable2b();
	long_variable3a();
	long_variable3b();
	too_long_variable1a();
	too_long_variable1b();
	too_long_variable1c();
	too_long_variable2a();
	too_long_variable2b();
	too_long_variable2c();
	too_long_variable3a();
	too_long_variable3b();
	too_long_variable3c();

	long_variable_name1();

	substitution_free(&varlist);
	substitution_free(&varlist2);

	return 0;
}
