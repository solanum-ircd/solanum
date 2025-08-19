/*
 * solanum - an advanced ircd.
 * Copyright (c) 2016 Ariadne Conill <ariadne@dereferenced.org>.
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

#include "stdinc.h"
#include "ircd_defs.h"
#include "msgbuf.h"
#include "client.h"
#include "ircd.h"

static const char tag_escape_table[256] = {
	/*        x0   x1   x2   x3   x4   x5   x6   x7   x8   x9   xA   xB   xC   xD   xE   xF */
	/* 0x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 'n',   0,   0, 'r',   0,   0,
	/* 1x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	/* 2x */ 's',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	/* 3x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, ':',   0,   0,   0,   0,
	/* 4x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	/* 5x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,'\\',   0,   0,   0,
};

static const char tag_unescape_table[256] = {
	/*        x0   x1   x2   x3   x4   x5   x6   x7   x8   x9   xA   xB   xC   xD   xE   xF */
	/* 0x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	/* 1x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	/* 2x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	/* 3x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, ';',   0,   0,   0,   0,   0,
	/* 4x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	/* 5x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,'\\',   0,   0,   0,
	/* 6x */   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,'\n',   0,
	/* 7x */   0,   0,'\r', ' ',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};

static void
msgbuf_unescape_value(char *value)
{
	char *in = value;
	char *out = value;

	if (value == NULL)
		return;

	while (*in != '\0') {
		if (*in == '\\') {
			const char unescape = tag_unescape_table[(unsigned char)*++in];

			/* "\\\0" is unescaped to the character itself, "\0" */
			if (*in == '\0')
				break;

			if (unescape) {
				*out++ = unescape;
				in++;
			} else {
				*out++ = *in++;
			}
		} else {
			*out++ = *in++;
		}
	}

	/* copy final '\0' */
	*out = *in;
}

/*
 * parse a message into a MsgBuf.
 * returns 0 on success, 1 on error.
 */
int
msgbuf_parse(struct MsgBuf *msgbuf, char *line)
{
	char *ch = line;

	msgbuf_init(msgbuf);

	if (*ch == '@') {
		char *t = ch + 1;

		ch = strchr(ch, ' ');
		if (ch == NULL)
			return 1;

		msgbuf->tagslen = ch - line + 1;

		/* truncate tags if they're too long */
		if (ch - line + 1 > TAGSLEN) {
			ch = &line[TAGSLEN - 1];
		}

		/* NULL terminate the tags string */
		*ch++ = '\0';

		while (1) {
			char *next = strchr(t, ';');
			char *eq = strchr(t, '=');

			if (next != NULL) {
				*next = '\0';

				if (eq > next)
					eq = NULL;
			}

			if (eq != NULL)
				*eq++ = '\0';

			if (*t != '\0') {
				msgbuf_unescape_value(eq);
				msgbuf_append_tag(msgbuf, t, eq, 0);
			}

			if (next != NULL) {
				t = next + 1;
			} else {
				break;
			}
		}
	}

	return msgbuf_partial_parse(msgbuf, ch);
}

int
msgbuf_partial_parse(struct MsgBuf *msgbuf, const char *line)
{
	char *ch = strdup(line);
	const char *start = ch;

	/* truncate message if it's too long */
	if (strlen(ch) > DATALEN) {
		ch[DATALEN] = '\0';
	}

	if (*ch == ':') {
		ch++;
		msgbuf->origin = ch;

		char *end = strchr(ch, ' ');
		if (end == NULL)
			return 4;

		*end = '\0';
		ch = end + 1;
	}

	if (*ch == '\0')
		return 2;

	msgbuf->endp = &ch[strlen(ch)];
	msgbuf->n_para = rb_string_to_array(ch, (char **)msgbuf->para, MAXPARA);
	if (msgbuf->n_para == 0)
		return 3;

	const char *potential_colon = msgbuf->para[msgbuf->n_para - 1] - 1;
	if (potential_colon >= start && *potential_colon == ':')
		msgbuf->preserve_trailing = true;
	msgbuf->cmd = msgbuf->para[0];
	return 0;
}

/*
 * Unparse the tail of a msgbuf perfectly, preserving framing details
 * msgbuf->para[n] will reach to the end of the line
 */

void
msgbuf_reconstruct_tail(struct MsgBuf *msgbuf, size_t n)
{
	if (msgbuf->endp == NULL || n > msgbuf->n_para)
		return;

	char *c;
	const char *c_;

	if (n == 0)
		c_ = msgbuf->para[n];
	else
		c_ = msgbuf->para[n-1] + strlen(msgbuf->para[n-1]) + 1;

	if (n == msgbuf->n_para && c_ == msgbuf->endp)
		return;

	msgbuf->para[n] = c_;
	/* promote to non-const. msgbuf->endp witnesses that this is allowed */
	c = msgbuf->endp - (msgbuf->endp - c_);

	for ( ; c < msgbuf->endp; c++)
	{
		if (*c == '\0')
			*c = ' ';
	}
}


/*
 * Unparse msgbuf tags into a buffer
 * returns the length of the tags written
 */
static size_t
msgbuf_unparse_tags(char *buf, size_t buflen, const struct MsgBuf *msgbuf, unsigned int capmask)
{
	bool has_tags = false;
	char *commit = buf;
	char *output = buf;
	const char * const end = &buf[buflen - 2]; /* this is where the final ' ' goes */

	for (int clientonly = 0; clientonly < 2; clientonly++)
	{
		const char *stop = commit + TAGSPARTLEN + 1;
		if (stop > end)
			stop = end;

		for (size_t i = 0; i < msgbuf->n_tags; i++)
		{
			output = commit;

			if ((msgbuf->tags[i].capmask & capmask) == 0)
				continue;

			if (msgbuf->tags[i].key == NULL)
				continue;

			size_t len = strlen(msgbuf->tags[i].key);
			if (len == 0 || (len == 1 && *msgbuf->tags[i].key == '+'))
				continue;

			/* spec requires that server tags are always provided before client-only tags */
			if (clientonly ^ (*msgbuf->tags[i].key == '+'))
				continue;

			if (output + len + 1 > stop)
				break;

			*output++ = has_tags ? ';' : '@';
			strcpy(output, msgbuf->tags[i].key);
			output += len;

			if (msgbuf->tags[i].value != NULL) {
				if (output >= stop)
					break;
				*output++ = '=';

				len = strlen(msgbuf->tags[i].value);
				/* this only checks the unescaped length,
				 * but the escaped length could be longer
				 */
				if (output + len > stop)
					break;

				for (size_t n = 0; n < len; n++) {
					const unsigned char c = msgbuf->tags[i].value[n];
					const char escape = tag_escape_table[c];

					if (escape) {
						if (output + 2 > stop)
							break;

						*output++ = '\\';
						*output++ = escape;
					} else {
						if (output >= stop)
							break;

						*output++ = c;
					}
				}
			}

			has_tags = true;
			commit = output;
		}
	}

	if (has_tags)
		*commit++ = ' ';

	*commit = 0;
	return commit - buf;
}

int
msgbuf_unparse_linebuf(char *buf, size_t buflen, void *data)
{
	struct MsgBuf_str_data *str_data = data;
	size_t used = 0;
	int ret;

	size_t tags_buflen = buflen;
	if (tags_buflen > TAGSLEN + 1)
		tags_buflen = TAGSLEN + 1;

	/* msgbuf_unparse / msgbuf_unparse_prefix have a couple of undesirable behaviors:
	 * 1. it enforces a source (using me.name if origin is NULL)
	 * 2. it adds in the cmd and target if defined (duplicating work done by msgbuf_unparse_para)
	 *
	 * As such, just unparse tags directly and then only add in origin if non-NULL
	 */
	used = msgbuf_unparse_tags(buf, tags_buflen, str_data->msgbuf, str_data->caps);
	if (buflen > used + DATALEN + 1)
		buflen = used + DATALEN + 1;

	if (str_data->msgbuf->origin != NULL)
	{
		/* rb_snprintf_append returns the complete length of the final string, not just the number of bytes it wrote */
		ret = rb_snprintf_append(buf, buflen, ":%s ", str_data->msgbuf->origin);
		if (ret > 0)
			used = ret;
	}

	used += msgbuf_unparse_para(buf, buflen, str_data->msgbuf);

	return used;
}

int
msgbuf_unparse_linebuf_tags(char *buf, size_t buflen, void *data)
{
	struct MsgBuf_str_data *str_data = data;
	if (buflen > TAGSLEN + 1)
		buflen = TAGSLEN + 1;

	return msgbuf_unparse_tags(buf, buflen, str_data->msgbuf, str_data->caps);
}

/*
 * unparse a MsgBuf and message prefix into a buffer
 * if origin is NULL, me.name will be used.
 * cmd should not be NULL.
 * updates buflen to correctly allow remaining message data to be added
 */
int
msgbuf_unparse_prefix(char *buf, size_t *buflen, const struct MsgBuf *msgbuf, unsigned int capmask)
{
	size_t tags_buflen;
	size_t used = 0;
	int ret;

	memset(buf, 0, *buflen);

	tags_buflen = *buflen;
	if (tags_buflen > TAGSLEN + 1)
		tags_buflen = TAGSLEN + 1;

	if (msgbuf->n_tags > 0)
		used = msgbuf_unparse_tags(buf, tags_buflen, msgbuf, capmask);

	const size_t data_bufmax = (used + DATALEN + 1);
	if (*buflen > data_bufmax)
		*buflen = data_bufmax;

	/* rb_snprintf_append returns the complete length of the final string, not just the number of bytes it wrote */
	ret = rb_snprintf_append(buf, *buflen, ":%s ", msgbuf->origin != NULL ? msgbuf->origin : me.name);
	if (ret > 0)
		used = ret;

	if (msgbuf->cmd != NULL) {
		ret = rb_snprintf_append(buf, *buflen, "%s ", msgbuf->cmd);
		if (ret > 0)
			used = ret;
	}

	if (msgbuf->target != NULL) {
		ret = rb_snprintf_append(buf, *buflen, "%s ", msgbuf->target);
		if (ret > 0)
			used = ret;
	}

	if (used > data_bufmax - 1)
		used = data_bufmax - 1;

	return used;
}

int
msgbuf_unparse_para(char *buf, size_t buflen, const struct MsgBuf *msgbuf)
{
	size_t orig_len = strlen(buf);
	size_t new_len = orig_len;
	int ret;

	for (size_t i = 0; i < msgbuf->n_para; i++) {
		const char *fmt;

		if (i == msgbuf->n_para - 1 && (msgbuf->preserve_trailing || strchr(msgbuf->para[i], ' ') != NULL || *msgbuf->para[i] == ':')) {
			fmt = i == 0 ? ":%s" : " :%s";
		} else {
			fmt = i == 0 ? "%s" : " %s";
		}

		/* rb_snprintf_append returns the complete length of the final string, not just the number of bytes it wrote */
		ret = rb_snprintf_append(buf, buflen, fmt, msgbuf->para[i]);
		if (ret > 0)
			new_len = ret;
	}

	if (new_len > buflen - 1)
		new_len = buflen - 1;

	return new_len - orig_len;
}

/*
 * unparse a pure MsgBuf into a buffer.
 * if origin is NULL, me.name will be used.
 * cmd should not be NULL.
 * returns 0 on success, 1 on error.
 */
int
msgbuf_unparse(char *buf, size_t buflen, const struct MsgBuf *msgbuf, unsigned int capmask)
{
	size_t buflen_copy = buflen;

	msgbuf_unparse_prefix(buf, &buflen_copy, msgbuf, capmask);
	msgbuf_unparse_para(buf, buflen_copy, msgbuf);

	return 0;
}

/*
 * unparse a MsgBuf stem + format string into a buffer
 * if origin is NULL, me.name will be used.
 * cmd may not be NULL.
 * returns 0 on success, 1 on error.
 */
int
msgbuf_vunparse_fmt(char *buf, size_t buflen, const struct MsgBuf *head, unsigned int capmask, const char *fmt, va_list va)
{
	size_t buflen_copy = buflen;
	char *ws;
	size_t prefixlen;

	msgbuf_unparse_prefix(buf, &buflen_copy, head, capmask);
	prefixlen = strlen(buf);

	ws = buf + prefixlen;
	vsnprintf(ws, buflen_copy - prefixlen, fmt, va);

	return 0;
}

/*
 * unparse a MsgBuf stem + format string into a buffer (with va_list handling)
 * if origin is NULL, me.name will be used.
 * cmd may not be NULL.
 * returns 0 on success, 1 on error.
 */
int
msgbuf_unparse_fmt(char *buf, size_t buflen, const struct MsgBuf *head, unsigned int capmask, const char *fmt, ...)
{
	va_list va;
	int res;

	va_start(va, fmt);
	res = msgbuf_vunparse_fmt(buf, buflen, head, capmask, fmt, va);
	va_end(va);

	return res;
}

const char *
msgbuf_get_tag(const struct MsgBuf *buf, const char *name)
{
	for (size_t i = 0; i < buf->n_tags; i++)
	{
		if (strcmp(name, buf->tags[i].key))
			continue;
		const char *v = buf->tags[i].value;
		return v != NULL ? v : "";
	}
	return NULL;
}

void
msgbuf_cache_init(struct MsgBuf_cache *cache, struct MsgBuf *msgbuf, const char *local_source, const char *remote_source)
{
	const char *orig_source = msgbuf->origin;
	struct MsgBuf_str_data data = { .msgbuf = msgbuf, .caps = 0 };
	rb_strf_t strings = { .func = msgbuf_unparse_linebuf, .func_args = &data, .length = DATALEN + 1, .next = NULL };

	memset(cache, 0, sizeof(struct MsgBuf_cache));
	cache->msgbuf = msgbuf;

	msgbuf->origin = local_source != NULL ? local_source : orig_source;
	rb_fsnprint(cache->local, sizeof(cache->local), &strings);

	msgbuf->origin = remote_source != NULL ? remote_source : orig_source;
	rb_fsnprint(cache->remote, sizeof(cache->remote), &strings);

	msgbuf->origin = orig_source;

	for (size_t i = 0; i < msgbuf->n_tags; i++) {
		cache->overall_capmask |= msgbuf->tags[i].capmask;
	}
}

buf_head_t*
msgbuf_cache_get(struct MsgBuf_cache *cache, unsigned int caps, bool is_remote)
{
	struct MsgBuf_cache_entry *entry = cache->head;
	struct MsgBuf_cache_entry *prev = NULL;
	struct MsgBuf_cache_entry *result = NULL;
	struct MsgBuf_cache_entry *tail = NULL;
	int n = 0;

	caps &= cache->overall_capmask;

	while (entry != NULL) {
		if (entry->caps == caps && entry->is_remote == is_remote) {
			/* Cache hit */
			result = entry;
			break;
		}

		tail = prev;
		prev = entry;
		entry = entry->next;
		n++;
	}

	if (result == NULL) {
		if (n < MSGBUF_CACHE_SIZE) {
			/* Cache miss, allocate a new entry */
			result = &cache->entry[n];
			prev = NULL;
		} else {
			/* Cache full, replace the last entry */
			result = prev;
			if (tail != NULL)
				tail->next = NULL;
			prev = NULL;

			rb_linebuf_donebuf(&result->linebuf);
		}

		/* Construct the line using the tags followed by the (already saved) message */
		struct MsgBuf_str_data msgbuf_str_data = { .msgbuf = cache->msgbuf, .caps = caps };
		rb_strf_t strings[2] = {
			{ .func = msgbuf_unparse_linebuf_tags, .func_args = &msgbuf_str_data, .next = &strings[1] },
			{ .format = is_remote ? cache->remote : cache->local, .format_args = NULL, .next = NULL },
		};

		result->caps = caps;
		result->is_remote = is_remote;
		rb_linebuf_newbuf(&result->linebuf);
		rb_linebuf_put(&result->linebuf, &strings[0]);
	}

	/* Move it to the top */
	if (cache->head != result) {
		if (prev != NULL) {
			prev->next = result->next;
		}
		result->next = cache->head;
		cache->head = result;
	}

	return &result->linebuf;
}

void
msgbuf_cache_free(struct MsgBuf_cache *cache)
{
	struct MsgBuf_cache_entry *entry = cache->head;

	while (entry != NULL) {
		rb_linebuf_donebuf(&entry->linebuf);
		entry = entry->next;
	}

	cache->head = NULL;
}
