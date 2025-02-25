/*
 *  rb_dictionary1.c: Test rb_dictionary
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

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__

#define MKTEXT(n) &text[sizeof(text) - ((n) + 1)]

static void replace1(void)
{
	rb_dictionary *dict = rb_dictionary_create("replace1", rb_strcasecmp);
	rb_dictionary_element *original = rb_dictionary_add(dict, "test", "data1");

	ok(original != NULL, MSG);
	is_string("data1", original->data, MSG);

#ifdef SOFT_ASSERT
	rb_dictionary_element *replacement = rb_dictionary_add(dict, "test", "data2");

	ok(replacement != NULL, MSG);
	ok(original == replacement, MSG);

	is_string("data2", original->data, MSG);
#endif
}

int main(int argc, char *argv[])
{
	rb_lib_init(NULL, NULL, NULL, 0, 1024, DNODE_HEAP_SIZE, FD_HEAP_SIZE);
	rb_linebuf_init(LINEBUF_HEAP_SIZE);

	plan_lazy();

	replace1();

	return 0;
}
