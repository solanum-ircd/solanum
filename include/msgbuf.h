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

#ifndef SOLANUM__MSGBUF_H
#define SOLANUM__MSGBUF_H

#define MAXPARA		(15)

/* a key-value structure for each message tag. */
struct MsgTag {
	const char *key;		/* the key of the tag (must be set) */
	const char *value;		/* the value of the tag or NULL */
	unsigned int capmask;		/* the capability mask this tag belongs to (used only when sending) */
};

struct MsgBuf {
	size_t n_tags;			/* the number of tags in the MsgBuf */
	struct MsgTag tags[MAXPARA];	/* the tags themselves, upto MAXPARA tags available */

	const char *origin;		/* the origin of the message (or NULL) */
	const char *target;		/* the target of the message (either NULL, or custom defined) */
	const char *cmd;		/* the cmd/verb of the message (either NULL, or para[0]) */
	char *endp;			/* one past the end of the original array */

	size_t n_para;			/* the number of parameters (always at least 1 if a full message) */
	const char *para[MAXPARA];	/* parameters vector (starting with cmd as para[0]) */
};

struct MsgBuf_str_data {
	const struct MsgBuf *msgbuf;
	unsigned int caps;
};

#define MSGBUF_CACHE_SIZE 32

struct MsgBuf_cache_entry {
	unsigned int caps;
	buf_head_t linebuf;
	struct MsgBuf_cache_entry *next;
};

struct MsgBuf_cache {
	const struct MsgBuf *msgbuf;
	char message[DATALEN + 1];
	unsigned int overall_capmask;

	/* Fixed maximum size linked list, new entries are allocated at the end
	 * of the array but are accessed through the "next" pointers.
	 *
	 * This does not use rb dlink to avoid unnecessary individual allocations.
	 */
	struct MsgBuf_cache_entry entry[MSGBUF_CACHE_SIZE];
	struct MsgBuf_cache_entry *head; /* LRU cache head */
};

/*
 * parse a message into a MsgBuf.
 * returns 0 on success, 1 on error.
 */
int msgbuf_parse(struct MsgBuf *msgbuf, char *line);

/*
 * Unparse the tail of a msgbuf perfectly, preserving framing details
 * msgbuf->para[n] will reach to the end of the line
 */
void msgbuf_reconstruct_tail(struct MsgBuf *msgbuf, size_t n);

/*
 * unparse a pure MsgBuf into a buffer.
 * if origin is NULL, me.name will be used.
 * cmd may not be NULL.
 * returns 0 on success, 1 on error.
 */
int msgbuf_unparse(char *buf, size_t buflen, const struct MsgBuf *msgbuf, unsigned int capmask);

int msgbuf_unparse_linebuf_tags(char *buf, size_t buflen, void *data);
int msgbuf_unparse_prefix(char *buf, size_t *buflen, const struct MsgBuf *msgbuf, unsigned int capmask);

void msgbuf_cache_init(struct MsgBuf_cache *cache, const struct MsgBuf *msgbuf, const rb_strf_t *message);
void msgbuf_cache_initf(struct MsgBuf_cache *cache, const struct MsgBuf *msgbuf, const rb_strf_t *message, const char *format, ...) AFP(4, 5);
buf_head_t *msgbuf_cache_get(struct MsgBuf_cache *cache, unsigned int caps);
void msgbuf_cache_free(struct MsgBuf_cache *cache);

static inline void
msgbuf_init(struct MsgBuf *msgbuf)
{
	memset(msgbuf, 0, sizeof(*msgbuf));
}

static inline void
msgbuf_append_tag(struct MsgBuf *msgbuf, const char *key, const char *value, unsigned int capmask)
{
	if (msgbuf->n_tags < MAXPARA) {
		msgbuf->tags[msgbuf->n_tags].key = key;
		msgbuf->tags[msgbuf->n_tags].value = value;
		msgbuf->tags[msgbuf->n_tags].capmask = capmask;
		msgbuf->n_tags++;
	}
}

#endif
