/*
 *  Copyright (C) 2020 Ed Kellett
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
#include "client.h"
#include "match.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__

struct Client me;

void match_arrange_stars(char *);

static void test_match(void)
{
	is_int(0, match("*foo*", "bar"), MSG);
	is_int(1, match("*foo*", "foo"), MSG);

	is_int(1, match("*foo*", "xfofoo"), MSG);
}

static void test_mask_match(void)
{

	is_int(0, mask_match("*foo*", "bar"), MSG);
	is_int(1, mask_match("*foo*", "foo"), MSG);

	is_int(1, mask_match("*foo*", "xfofoo"), MSG);

	is_int(1, mask_match("*", "*foo*"), MSG);
	is_int(0, mask_match("*foo*", "*"), MSG);
	is_int(1, mask_match("*", "*"), MSG);
	is_int(0, mask_match("?", "*"), MSG);
	is_int(1, mask_match("*?", "*?"), MSG);
	is_int(1, mask_match("?*", "*?"), MSG);
	is_int(1, mask_match("*?*?*?*", "*?????*"), MSG);
	is_int(0, mask_match("*??*??*??*", "*?????*"), MSG);

	is_int(1, mask_match("?*", "*a"), MSG);
	is_int(1, mask_match("???*", "*a*a*a"), MSG);
	is_int(0, mask_match("???*", "*a*a*"), MSG);

	is_int(0, mask_match("??", "a"), MSG);
	is_int(1, mask_match("??", "aa"), MSG);
	is_int(0, mask_match("??", "aaa"), MSG);
}

static void test_arrange_stars(void)
{
	{
		char rearrange[] = "quick brown fox";
		match_arrange_stars(rearrange);
		is_string("quick brown fox", rearrange, MSG);
	}
	{
		char rearrange[] = "?*?*?*";
		match_arrange_stars(rearrange);
		is_string("***???", rearrange, MSG);
	}
	{
		char rearrange[] = "?*? *?*";
		match_arrange_stars(rearrange);
		is_string("*?? **?", rearrange, MSG);
	}
}

int main(int argc, char *argv[])
{
	plan_lazy();

	test_match();
	test_mask_match();
	test_arrange_stars();

	return 0;
}
