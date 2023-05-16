/*
 * Solanum: a slightly advanced ircd
 * privilege.h: Dynamic privileges API.
 *
 * Copyright (c) 2021 Ed Kellett <e@kellett.im>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 * Copyright (c) 2008 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __SOLANUM_PRIVILEGE_H
#define __SOLANUM_PRIVILEGE_H

#include "stdinc.h"

struct Client;

enum {
	PRIV_NEEDOPER = 1
};
typedef unsigned int PrivilegeFlags;

struct PrivilegeSet {
	rb_dlink_node node;
	size_t size;
	const char **privs;
	size_t stored_size, allocated_size;
	char *priv_storage;
	char *name;
	struct PrivilegeSet *shadow;
	PrivilegeFlags flags;
	unsigned int status;	/* If CONF_ILLEGAL, delete when no refs */
	int refs;
};

struct privset_diff {
	const struct PrivilegeSet *unchanged;
	const struct PrivilegeSet *added;
	const struct PrivilegeSet *removed;
};

bool privilegeset_in_set(const struct PrivilegeSet *set, const char *priv);
const char *const *privilegeset_privs(const struct PrivilegeSet *set);
struct PrivilegeSet *privilegeset_set_new(const char *name, const char *privs, PrivilegeFlags flags);
struct PrivilegeSet *privilegeset_extend(const struct PrivilegeSet *parent, const char *name, const char *privs, PrivilegeFlags flags);
struct PrivilegeSet *privilegeset_get(const char *name);
struct PrivilegeSet *privilegeset_ref(struct PrivilegeSet *set);
void privilegeset_unref(struct PrivilegeSet *set);
void privilegeset_prepare_rehash(void);
void privilegeset_cleanup_rehash(void);
void privilegeset_report(struct Client *source_p);

struct privset_diff privilegeset_diff(const struct PrivilegeSet *, const struct PrivilegeSet *);

#endif
