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
#include "privilege.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__

void privilegeset_add_privs(struct PrivilegeSet *dst, const char *privs);

struct Client me;

static void cleanup(void)
{
	privilegeset_prepare_rehash();
	privilegeset_cleanup_rehash();
}

static void test_privset_membership(void)
{
	struct PrivilegeSet *set = privilegeset_set_new("test", "foo bar", 0);

	is_bool(true, privilegeset_in_set(set, "foo"), MSG);
	is_bool(true, privilegeset_in_set(set, "bar"), MSG);

	is_bool(false, privilegeset_in_set(set, "qux"), MSG);

	cleanup();
}

static void test_privset_add(void)
{
	struct PrivilegeSet *set = privilegeset_set_new("test", "foo bar", 0);
	privilegeset_add_privs(set, "baz qux");

	is_bool(true, privilegeset_in_set(set, "foo"), MSG);
	is_bool(true, privilegeset_in_set(set, "bar"), MSG);
	is_bool(true, privilegeset_in_set(set, "baz"), MSG);
	is_bool(true, privilegeset_in_set(set, "qux"), MSG);

	is_bool(false, privilegeset_in_set(set, "frob"), MSG);

	cleanup();
}

static void test_privset_extend(void)
{
	struct PrivilegeSet *parent = privilegeset_set_new("parent", "foo bar", 0);
	struct PrivilegeSet *child = privilegeset_extend(parent, "child", "qux", 0);

	is_bool(true, privilegeset_in_set(child, "foo"), MSG);
	is_bool(true, privilegeset_in_set(child, "bar"), MSG);

	is_bool(false, privilegeset_in_set(parent, "qux"), MSG);
	is_bool(true, privilegeset_in_set(child, "qux"), MSG);

	cleanup();
}

static void test_privset_persistence(void)
{
	struct PrivilegeSet *set = privilegeset_set_new("test", "foo", 0);
	privilegeset_ref(set);

	/* should survive rehash since it's referenced, but become empty */
	privilegeset_prepare_rehash();
	privilegeset_cleanup_rehash();
	is_bool(false, privilegeset_in_set(set, "foo"), MSG);

	/* and have its contents replaced by the equal name */
	privilegeset_set_new("test", "bar", 0);
	is_bool(true, privilegeset_in_set(set, "bar"), MSG);

	privilegeset_unref(set);
	cleanup();
}

static void test_privset_diff(void)
{
	struct PrivilegeSet *old = privilegeset_set_new("old", "foo bar", 0);
	struct PrivilegeSet *new = privilegeset_set_new("new", "foo qux", 0);
	struct privset_diff diff = privilegeset_diff(old, new);

	is_bool(true, privilegeset_in_set(diff.unchanged, "foo"), MSG);
	is_bool(false, privilegeset_in_set(diff.added, "foo"), MSG);
	is_bool(false, privilegeset_in_set(diff.removed, "foo"), MSG);

	is_bool(false, privilegeset_in_set(diff.unchanged, "bar"), MSG);
	is_bool(false, privilegeset_in_set(diff.added, "bar"), MSG);
	is_bool(true, privilegeset_in_set(diff.removed, "bar"), MSG);

	is_bool(false, privilegeset_in_set(diff.unchanged, "qux"), MSG);
	is_bool(true, privilegeset_in_set(diff.added, "qux"), MSG);
	is_bool(false, privilegeset_in_set(diff.removed, "qux"), MSG);

	cleanup();
}

static void test_privset_diff_rehash(void)
{
	struct PrivilegeSet *set = privilegeset_set_new("test", "foo bar", 0);
	struct privset_diff diff;
	privilegeset_ref(set);

	privilegeset_prepare_rehash();

	/* should have changed from foo, bar to nothing, i.e. -foo -bar */
	diff = privilegeset_diff(set->shadow, set);

	is_bool(false, privilegeset_in_set(diff.unchanged, "foo"), MSG);
	is_bool(false, privilegeset_in_set(diff.added, "foo"), MSG);
	is_bool(true, privilegeset_in_set(diff.removed, "foo"), MSG);

	is_bool(false, privilegeset_in_set(diff.unchanged, "bar"), MSG);
	is_bool(false, privilegeset_in_set(diff.added, "bar"), MSG);
	is_bool(true, privilegeset_in_set(diff.removed, "bar"), MSG);

	privilegeset_set_new("test", "foo qux", 0);
	diff = privilegeset_diff(set->shadow, set);

	/* should have changed from foo, bar to foo, qux, i.e. =foo -bar +qux */
	is_bool(true, privilegeset_in_set(diff.unchanged, "foo"), MSG);
	is_bool(false, privilegeset_in_set(diff.added, "foo"), MSG);
	is_bool(false, privilegeset_in_set(diff.removed, "foo"), MSG);

	is_bool(false, privilegeset_in_set(diff.unchanged, "bar"), MSG);
	is_bool(false, privilegeset_in_set(diff.added, "bar"), MSG);
	is_bool(true, privilegeset_in_set(diff.removed, "bar"), MSG);

	is_bool(false, privilegeset_in_set(diff.unchanged, "qux"), MSG);
	is_bool(true, privilegeset_in_set(diff.added, "qux"), MSG);
	is_bool(false, privilegeset_in_set(diff.removed, "qux"), MSG);

	privilegeset_cleanup_rehash();

	privilegeset_unref(set);
	cleanup();
}

int main(int argc, char *argv[])
{
	plan_lazy();

	test_privset_membership();
	test_privset_add();
	test_privset_extend();
	test_privset_persistence();
	test_privset_diff();
	test_privset_diff_rehash();

	return 0;
}
