/*
 * Solanum: a slightly advanced ircd
 * privilege.c: Dynamic privileges API.
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
 * Copyright (c) 2008 William Pitcock <nenolod@dereferenced.org>
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

#include <stdinc.h>
#include "s_conf.h"
#include "privilege.h"
#include "numeric.h"
#include "s_assert.h"
#include "logger.h"
#include "send.h"

static rb_dlink_list privilegeset_list = {NULL, NULL, 0};

static struct PrivilegeSet *
privilegeset_get_any(const char *name)
{
	rb_dlink_node *iter;

	s_assert(name != NULL);

	RB_DLINK_FOREACH(iter, privilegeset_list.head)
	{
		struct PrivilegeSet *set = (struct PrivilegeSet *) iter->data;

		if (!rb_strcasecmp(set->name, name))
			return set;
	}

	return NULL;
}

static int
privilegeset_cmp_priv(const void *a_, const void *b_)
{
	const char *const *a = a_, *const *b = b_;
	return strcmp(*a, *b);
}

static void
privilegeset_index(struct PrivilegeSet *set)
{
	size_t n;
	const char *s;
	const char **p;

	rb_free(set->privs);

	set->privs = rb_malloc(sizeof *set->privs * (set->size + 1));
	p = set->privs;

	for (n = 0, s = set->priv_storage; n < set->size; n++, s += strlen(s) + 1)
		*p++ = s;
	qsort(set->privs, set->size, sizeof *set->privs, privilegeset_cmp_priv);
	set->privs[set->size] = NULL;
}

static void
privilegeset_add_privs(struct PrivilegeSet *dst, const char *privs)
{
	size_t alloc_size;
	size_t n;

	if (dst->priv_storage == NULL)
	{
		dst->stored_size = dst->allocated_size = 0;
		alloc_size = 256;
	}
	else
	{
		alloc_size = dst->allocated_size;
	}

	dst->stored_size += strlen(privs) + 1;

	while (alloc_size < dst->stored_size)
		alloc_size *= 2;

	if (alloc_size > dst->allocated_size)
		dst->priv_storage = rb_realloc(dst->priv_storage, alloc_size);

	dst->allocated_size = alloc_size;

	const char *s;
	char *d;
	for (s = privs, d = dst->priv_storage; s < privs + strlen(privs); s += n , d += n)
	{
		const char *e = strchr(s, ' ');
		/* up to space if there is one, else up to end of string */
		n = 1 + (e != NULL ? e - s : strlen(s));
		rb_strlcpy(d, s, n);

		dst->size += 1;
	}

	privilegeset_index(dst);
}

static void
privilegeset_add_privilegeset(struct PrivilegeSet *dst, const struct PrivilegeSet *src)
{
	size_t cur_size, alloc_size;

	if (dst->priv_storage == NULL)
	{
		dst->stored_size = dst->allocated_size = 0;
		cur_size = 0;
		alloc_size = 256;
	}
	else
	{
		cur_size = dst->stored_size;
		alloc_size = dst->allocated_size;
	}

	dst->stored_size = cur_size + src->stored_size;

	while (alloc_size < dst->stored_size)
		alloc_size *= 2;

	if (alloc_size > dst->allocated_size)
		dst->priv_storage = rb_realloc(dst->priv_storage, alloc_size);

	dst->allocated_size = alloc_size;

	memcpy(dst->priv_storage + cur_size, src->priv_storage, src->stored_size);
	dst->size += src->size;

	privilegeset_index(dst);
}

static struct PrivilegeSet *
privilegeset_new_orphan(const char *name)
{
	struct PrivilegeSet *set;
	set = rb_malloc(sizeof *set);
	*set = (struct PrivilegeSet) {
		.size = 0,
		.privs = NULL,
		.priv_storage = NULL,
		.shadow = NULL,
		.status = 0,
		.refs = 0,
		.name = rb_strdup(name),
	};
	return set;
}

static void
privilegeset_free(struct PrivilegeSet *set)
{
	if (set == NULL)
		return;

	privilegeset_free(set->shadow);
	rb_free(set->name);
	rb_free(set->privs);
	rb_free(set->priv_storage);
	rb_free(set);
}

static void
privilegeset_shade(struct PrivilegeSet *set)
{
	privilegeset_free(set->shadow);

	set->shadow = privilegeset_new_orphan(set->name);
	set->shadow->privs = set->privs;
	set->shadow->size = set->size;
	set->shadow->priv_storage = set->priv_storage;
	set->shadow->stored_size = set->stored_size;
	set->shadow->allocated_size = set->allocated_size;

	set->privs = NULL;
	set->size = 0;
	set->priv_storage = NULL;
	set->stored_size = 0;
	set->allocated_size = 0;
}

static void
privilegeset_clear(struct PrivilegeSet *set)
{
	rb_free(set->privs);
	set->privs = NULL;
	set->size = 0;
	set->stored_size = 0;
}

bool
privilegeset_in_set(const struct PrivilegeSet *set, const char *priv)
{
	s_assert(set != NULL);
	s_assert(priv != NULL);

	const char **found = bsearch(&priv, set->privs, set->size, sizeof *set->privs, privilegeset_cmp_priv);
	return found != NULL;
}

const char **
privilegeset_privs(const struct PrivilegeSet *set)
{
	static const char *no_privs[] = { NULL };
	return set->privs != NULL ? set->privs : no_privs;
}

struct PrivilegeSet *
privilegeset_set_new(const char *name, const char *privs, PrivilegeFlags flags)
{
	struct PrivilegeSet *set;

	set = privilegeset_get_any(name);
	if (set != NULL)
	{
		if (!(set->status & CONF_ILLEGAL))
			ilog(L_MAIN, "Duplicate privset %s", name);
		set->status &= ~CONF_ILLEGAL;
		privilegeset_clear(set);
	}
	else
	{
		set = privilegeset_new_orphan(name);
		rb_dlinkAdd(set, &set->node, &privilegeset_list);
	}
	privilegeset_add_privs(set, privs);
	set->flags = flags;

	return set;
}

struct PrivilegeSet *
privilegeset_extend(const struct PrivilegeSet *parent, const char *name, const char *privs, PrivilegeFlags flags)
{
	struct PrivilegeSet *set;

	s_assert(parent != NULL);
	s_assert(name != NULL);
	s_assert(privs != NULL);

	set = privilegeset_set_new(name, privs, flags);
	privilegeset_add_privilegeset(set, parent);
	set->flags = flags;

	return set;
}

struct PrivilegeSet *
privilegeset_get(const char *name)
{
	struct PrivilegeSet *set;

	set = privilegeset_get_any(name);
	if (set != NULL && set->status & CONF_ILLEGAL)
		set = NULL;
	return set;
}

struct PrivilegeSet *
privilegeset_ref(struct PrivilegeSet *set)
{
	s_assert(set != NULL);

	set->refs++;

	return set;
}

void
privilegeset_unref(struct PrivilegeSet *set)
{
	s_assert(set != NULL);

	if (set->refs > 0)
		set->refs--;
	else
		ilog(L_MAIN, "refs on privset %s is already 0",
				set->name);
	if (set->refs == 0 && set->status & CONF_ILLEGAL)
	{
		rb_dlinkDelete(&set->node, &privilegeset_list);

		privilegeset_free(set);
	}
}

const struct PrivilegeSet **
privilegeset_diff(const struct PrivilegeSet *old, const struct PrivilegeSet *new)
{
	static const char *no_privs[] = { NULL };
	static const struct PrivilegeSet empty = { .size = 0, .privs = no_privs };
	static struct PrivilegeSet *set_unchanged = NULL,
	                           *set_added = NULL,
	                           *set_removed = NULL;
	static const struct PrivilegeSet *result_sets[3];
	static size_t n_privs = 0;
	size_t new_size = n_privs ? n_privs : 32;
	size_t i = 0, j = 0;

	if (result_sets[0] == NULL)
	{
		result_sets[0] = set_unchanged = privilegeset_new_orphan("<unchanged>");
		result_sets[1] = set_added = privilegeset_new_orphan("<added>");
		result_sets[2] = set_removed = privilegeset_new_orphan("<removed>");
	}

	if (old == NULL)
		old = &empty;
	if (new == NULL)
		new = &empty;

	while (new_size < MAX(old->size, new->size) + 1)
		new_size *= 2;

	if (new_size > n_privs)
	{
		set_unchanged->privs = rb_realloc(set_unchanged->privs, sizeof *set_unchanged->privs * new_size);
		set_added->privs = rb_realloc(set_added->privs, sizeof *set_added->privs * new_size);
		set_removed->privs = rb_realloc(set_removed->privs, sizeof *set_removed->privs * new_size);
	}

	const char **res_unchanged = set_unchanged->privs;
	const char **res_added = set_added->privs;
	const char **res_removed = set_removed->privs;

	while (i < old->size || j < new->size)
	{
		const char *oldpriv = NULL, *newpriv = NULL;
		int ord = 0;
		oldpriv = privilegeset_privs(old)[i];
		newpriv = privilegeset_privs(new)[j];

		if (oldpriv && newpriv)
			ord = strcmp(oldpriv, newpriv);

		if (newpriv == NULL || ord < 0)
		{
			*res_removed++ = oldpriv;
			i++;
		}
		else if (oldpriv == NULL || ord > 0)
		{
			*res_added++ = newpriv;
			j++;
		}
		else
		{
			*res_unchanged++ = oldpriv;
			i++; j++;
		}
	}

	*res_removed = *res_added = *res_unchanged = NULL;
	set_unchanged->size = res_unchanged - set_unchanged->privs;
	set_added->size = res_added - set_added->privs;
	set_removed->size = res_removed - set_removed->privs;

	return result_sets;
}

void
privilegeset_prepare_rehash()
{
	rb_dlink_node *iter;

	RB_DLINK_FOREACH(iter, privilegeset_list.head)
	{
		struct PrivilegeSet *set = iter->data;

		/* the "default" privset is special and must remain available */
		if (!strcmp(set->name, "default"))
			continue;

		set->status |= CONF_ILLEGAL;
		privilegeset_shade(set);
	}
}

void
privilegeset_cleanup_rehash()
{
	rb_dlink_node *iter, *next;

	RB_DLINK_FOREACH_SAFE(iter, next, privilegeset_list.head)
	{
		struct PrivilegeSet *set = iter->data;

		if (set->shadow)
		{
			privilegeset_free(set->shadow);
			set->shadow = NULL;
		}

		privilegeset_ref(set);
		privilegeset_unref(set);
	}
}

void
privilegeset_report(struct Client *source_p)
{
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, privilegeset_list.head)
	{
		struct PrivilegeSet *set = ptr->data;

		/* use RPL_STATSDEBUG for now -- jilles */
		send_multiline_init(source_p, " ", ":%s %03d %s O :%s ",
				get_id(&me, source_p),
				RPL_STATSDEBUG,
				get_id(source_p, source_p),
				set->name);
		send_multiline_remote_pad(source_p, &me);
		send_multiline_remote_pad(source_p, source_p);
		for (const char **s = privilegeset_privs(set); *s != NULL; s++)
			send_multiline_item(source_p, "%s", *s);
		send_multiline_fini(source_p, NULL);
	}
}
