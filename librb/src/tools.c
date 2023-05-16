/*
 *  ircd-ratbox: A slightly useful ircd.
 *  tools.c: Various functions needed here and there.
 *
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
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
 *
 *
 *  Here is the original header:
 *
 *  Useful stuff, ripped from places ..
 *  adrian chadd <adrian@creative.net.au>
 *
 *  The TOOLS_C define builds versions of the functions in tools.h
 *  so that they end up in the resulting object files.  If its not
 *  defined, tools.h will build inlined versions of the functions
 *  on supported compilers
 */

#define _GNU_SOURCE 1
#include <librb_config.h>
#include <rb_lib.h>
#include <rb_tools.h>


/*
 * init_rb_dlink_nodes
 *
 */
static rb_bh *dnode_heap;
void
rb_init_rb_dlink_nodes(size_t dh_size)
{

	dnode_heap = rb_bh_create(sizeof(rb_dlink_node), dh_size, "librb_dnode_heap");
	if(dnode_heap == NULL)
		rb_outofmemory();
}

/*
 * make_rb_dlink_node
 *
 * inputs	- NONE
 * output	- pointer to new rb_dlink_node
 * side effects	- NONE
 */
rb_dlink_node *
rb_make_rb_dlink_node(void)
{
	return (rb_bh_alloc(dnode_heap));
}

/*
 * free_rb_dlink_node
 *
 * inputs	- pointer to rb_dlink_node
 * output	- NONE
 * side effects	- free given rb_dlink_node
 */
void
rb_free_rb_dlink_node(rb_dlink_node *ptr)
{
	assert(ptr != NULL);
	rb_bh_free(dnode_heap, ptr);
}

/* rb_string_to_array()
 *   Changes a given buffer into an array of parameters.
 *   Taken from ircd-ratbox.
 *
 * inputs	- string to parse, array to put in (size >= maxpara)
 * outputs	- number of parameters
 */
int
rb_string_to_array(char *string, char **parv, int maxpara)
{
	char *p, *xbuf = string;
	int x = 0;

	if(string == NULL || string[0] == '\0')
		return x;

	while(*xbuf == ' ')	/* skip leading spaces */
		xbuf++;
	if(*xbuf == '\0')	/* ignore all-space args */
		return x;

	do
	{
		if(*xbuf == ':')	/* Last parameter */
		{
			xbuf++;
			parv[x++] = xbuf;
			return x;
		}
		else
		{
			parv[x++] = xbuf;
			if((p = strchr(xbuf, ' ')) != NULL)
			{
				*p++ = '\0';
				xbuf = p;
			}
			else
				return x;
		}
		while(*xbuf == ' ')
			xbuf++;
		if(*xbuf == '\0')
			return x;
	}
	while(x < maxpara - 1);

	if(*p == ':')
		p++;

	parv[x++] = p;
	return x;
}

int
rb_strcasecmp(const char *s1, const char *s2)
{
	return strcasecmp(s1, s2);
}

int
rb_strncasecmp(const char *s1, const char *s2, size_t n)
{
	return strncasecmp(s1, s2, n);
}

#ifndef HAVE_STRCASESTR
/* Fallback taken from FreeBSD. --Elizafox */
char *
rb_strcasestr(const char *s, const char *find)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != 0) {
		c = tolower((unsigned char)c);
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while ((char)tolower((unsigned char)sc) != c);
		} while (rb_strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}
#else
char *
rb_strcasestr(const char *s, const char *find)
{
	return strcasestr(s, find);
}
#endif

#ifndef HAVE_STRLCAT
size_t
rb_strlcat(char *dest, const char *src, size_t count)
{
	size_t dsize = strlen(dest);
	size_t len = strlen(src);
	size_t res = dsize + len;

	dest += dsize;
	count -= dsize;
	if(len >= count)
		len = count - 1;
	memcpy(dest, src, len);
	dest[len] = 0;
	return res;
}
#else
size_t
rb_strlcat(char *dest, const char *src, size_t count)
{
	return strlcat(dest, src, count);
}
#endif

#ifndef HAVE_STRLCPY
size_t
rb_strlcpy(char *dest, const char *src, size_t size)
{
	size_t ret = strlen(src);

	if(size)
	{
		size_t len = (ret >= size) ? size - 1 : ret;
		memcpy(dest, src, len);
		dest[len] = '\0';
	}
	return ret;
}
#else
size_t
rb_strlcpy(char *dest, const char *src, size_t size)
{
	return strlcpy(dest, src, size);
}
#endif


size_t
rb_strnlen(const char *s, size_t count)
{
	return strnlen(s, count);
}

/*
 * rb_snprintf_append()
 * appends snprintf formatted string to the end of the buffer but not
 * exceeding len
 */
int
rb_snprintf_append(char *str, size_t len, const char *format, ...)
{
	if(len == 0)
		return -1;

	int orig_len = strlen(str);

	if((int)len < orig_len)
	{
		str[len - 1] = '\0';
		return len - 1;
	}

	va_list ap;
	va_start(ap, format);
	int append_len = vsnprintf(str + orig_len, len - orig_len, format, ap);
	va_end(ap);

	if (append_len < 0)
		return append_len;

	return (orig_len + append_len);
}

/*
 * rb_snprintf_try_append()
 * appends snprintf formatted string to the end of the buffer but not
 * exceeding len
 * returns -1 if there isn't enough space for the whole string to fit
 */
int
rb_snprintf_try_append(char *str, size_t len, const char *format, ...)
{
	if(len == 0)
		return -1;

	int orig_len = strlen(str);

	if((int)len < orig_len) {
		str[len - 1] = '\0';
		return -1;
	}

	va_list ap;
	va_start(ap, format);
	int append_len = vsnprintf(str + orig_len, len - orig_len, format, ap);
	va_end(ap);

	if (append_len < 0)
		return append_len;

	if (orig_len + append_len > (int)(len - 1)) {
		str[orig_len] = '\0';
		return -1;
	}

	return (orig_len + append_len);
}

/* rb_basename
 *
 * input        -
 * output       -
 * side effects -
 */
char *
rb_basename(const char *path)
{
        const char *s;

        if(!(s = strrchr(path, '/')))
                s = path;
        else
                s++;
	return rb_strdup(s);
}

/*
 * rb_dirname
 */

char *
rb_dirname (const char *path)
{
	char *s;

	s = strrchr(path, '/');
	if(s == NULL)
	{
		return rb_strdup(".");
	}

	/* remove extra slashes */
	while(s > path && *s == '/')
		--s;

	return rb_strndup(path, ((uintptr_t)s - (uintptr_t)path) + 2);
}



int rb_fsnprint(char *buf, size_t len, const rb_strf_t *strings)
{
	size_t used = 0;
	size_t remaining = len;

	while (strings != NULL) {
		int ret = 0;

		if (strings->length != 0) {
			remaining = strings->length;
			if (remaining > len - used)
				remaining = len - used;
		}

		if (remaining == 0)
			break;

		if (strings->format != NULL) {
			if (strings->format_args != NULL) {
				ret = vsnprintf(buf + used, remaining,
					strings->format, *strings->format_args);
			} else {
				ret = rb_strlcpy(buf + used,
					strings->format, remaining);
			}
		} else if (strings->func != NULL) {
			ret = strings->func(buf + used, remaining,
				strings->func_args);
		}

		if (ret < 0) {
			return ret;
		} else if ((size_t)ret > remaining - 1) {
			used += remaining - 1;
		} else {
			used += ret;
		}

		if (used >= len - 1) {
			used = len - 1;
			break;
		}

		remaining -= ret;
		strings = strings->next;
	}

	return used;
}

int rb_fsnprintf(char *buf, size_t len, const rb_strf_t *strings, const char *format, ...)
{
	va_list args;
	rb_strf_t prepend_string = { .format = format, .format_args = &args, .next = strings };
	int ret;

	va_start(args, format);
	ret = rb_fsnprint(buf, len, &prepend_string);
	va_end(args);

	return ret;
}

