/*
 *  ircd-ratbox: A slightly useful ircd.
 *  send.c: Functions for sending messages.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

#include "stdinc.h"
#include "send.h"
#include "channel.h"
#include "class.h"
#include "client.h"
#include "match.h"
#include "ircd.h"
#include "numeric.h"
#include "s_assert.h"
#include "s_serv.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "logger.h"
#include "hook.h"
#include "monitor.h"
#include "msgbuf.h"

/* send the message to the link the target is attached to */
#define send_linebuf(a,b) _send_linebuf((a->from ? a->from : a) ,b)

#define CLIENT_CAP_MASK(x)	(MyClient((x)) ? (x)->localClient->caps : \
		((x)->from == &me || ((x)->from && (x)->from->localClient->caps & CAP_STAG)) ? (unsigned)-1 : 0)

static void send_queued_write(rb_fde_t *F, void *data);

unsigned long current_serial = 0L;

struct Client *remote_rehash_oper_p;

/* send_linebuf()
 *
 * inputs	- client to send to, linebuf to attach
 * outputs	-
 * side effects - linebuf is attached to client
 */
static int
_send_linebuf(struct Client *to, buf_head_t *linebuf)
{
	if(IsMe(to))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "Trying to send message to myself!");
		return 0;
	}

	if(!MyConnect(to) || IsIOError(to))
		return 0;

	if(rb_linebuf_len(&to->localClient->buf_sendq) > get_sendq(to))
	{
		dead_link(to, 1);

		if(IsServer(to))
		{
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
					     "Max SendQ limit exceeded for %s: %u > %lu",
					     to->name,
					     rb_linebuf_len(&to->localClient->buf_sendq),
					     get_sendq(to));

			ilog(L_SERVER, "Max SendQ limit exceeded for %s: %u > %lu",
			     log_client_name(to, SHOW_IP),
			     rb_linebuf_len(&to->localClient->buf_sendq),
			     get_sendq(to));
		}

		return -1;
	}
	else
	{
		/* just attach the linebuf to the sendq instead of
		 * generating a new one
		 */
		rb_linebuf_attach(&to->localClient->buf_sendq, linebuf);
	}

	/*
	 ** Update statistics. The following is slightly incorrect
	 ** because it counts messages even if queued, but bytes
	 ** only really sent. Queued bytes get updated in SendQueued.
	 */
	to->localClient->sendM += 1;
	me.localClient->sendM += 1;
	if(rb_linebuf_len(&to->localClient->buf_sendq) > 0)
		send_queued(to);
	return 0;
}

/* send_linebuf_remote()
 *
 * inputs	- client to attach to, sender, linebuf
 * outputs	-
 * side effects - client has linebuf attached
 */
static void
send_linebuf_remote(struct Client *to, struct Client *from, buf_head_t *linebuf)
{
	if(to->from)
		to = to->from;

	/* we assume the caller has already tested for fake direction */
	_send_linebuf(to, linebuf);
}

/* send_queued_write()
 *
 * inputs	- fd to have queue sent, client we're sending to
 * outputs	- contents of queue
 * side effects - write is rescheduled if queue isnt emptied
 */
void
send_queued(struct Client *to)
{
	int retlen;

	rb_fde_t *F = to->localClient->F;
	if (!F)
		return;

	/* cant write anything to a dead socket. */
	if(IsIOError(to))
		return;

	/* try to flush later when the write event resets this */
	if(IsFlush(to))
		return;

	if(rb_linebuf_len(&to->localClient->buf_sendq))
	{
		while ((retlen =
			rb_linebuf_flush(F, &to->localClient->buf_sendq)) > 0)
		{
			/* We have some data written .. update counters */
			ClearFlush(to);

			to->localClient->sendB += retlen;
			me.localClient->sendB += retlen;
			if(to->localClient->sendB > 1023)
			{
				to->localClient->sendK += (to->localClient->sendB >> 10);
				to->localClient->sendB &= 0x03ff;	/* 2^10 = 1024, 3ff = 1023 */
			}
			else if(me.localClient->sendB > 1023)
			{
				me.localClient->sendK += (me.localClient->sendB >> 10);
				me.localClient->sendB &= 0x03ff;
			}
		}

		if(retlen == 0 || (retlen < 0 && !rb_ignore_errno(errno)))
		{
			dead_link(to, 0);
			return;
		}
	}

	if(rb_linebuf_len(&to->localClient->buf_sendq))
	{
		SetFlush(to);
		rb_setselect(to->localClient->F, RB_SELECT_WRITE,
			       send_queued_write, to);
	}
	else
		ClearFlush(to);
}

void
send_pop_queue(struct Client *to)
{
	if(to->from != NULL)
		to = to->from;
	if(!MyConnect(to) || IsIOError(to))
		return;
	if(rb_linebuf_len(&to->localClient->buf_sendq) > 0)
		send_queued(to);
}

/* send_queued_write()
 *
 * inputs	- fd to have queue sent, client we're sending to
 * outputs	- contents of queue
 * side effects - write is scheduled if queue isnt emptied
 */
static void
send_queued_write(rb_fde_t *F, void *data)
{
	struct Client *to = data;
	ClearFlush(to);
	send_queued(to);
}

/*
 * linebuf_put_*
 *
 * inputs       - msgbuf header, linebuf object, capability mask, pattern, arguments
 * outputs      - none
 * side effects - the linebuf object is cleared, then populated
 */
static void
linebuf_put_tags(buf_head_t *linebuf, const struct MsgBuf *msgbuf, const struct Client *target_p, rb_strf_t *message)
{
	struct MsgBuf_str_data msgbuf_str_data = { .msgbuf = msgbuf, .caps = CLIENT_CAP_MASK(target_p) };
	rb_strf_t strings = { .func = msgbuf_unparse_linebuf_tags, .func_args = &msgbuf_str_data, .length = TAGSLEN + 1, .next = message };

	message->length = DATALEN + 1;
	rb_linebuf_put(linebuf, &strings);
}

static void
linebuf_put_tagsf(buf_head_t *linebuf, const struct MsgBuf *msgbuf, const struct Client *target_p, const rb_strf_t *message, const char *format, ...)
{
	va_list va;
	rb_strf_t strings = { .format = format, .format_args = &va, .next = message };

	va_start(va, format);
	linebuf_put_tags(linebuf, msgbuf, target_p, &strings);
	va_end(va);
}

static void
linebuf_put_msg(buf_head_t *linebuf, rb_strf_t *message)
{
	message->length = DATALEN + 1;
	rb_linebuf_put(linebuf, message);
}

static void
linebuf_put_msgf(buf_head_t *linebuf, const rb_strf_t *message, const char *format, ...)
{
	va_list va;
	rb_strf_t strings = { .format = format, .format_args = &va, .next = message };

	va_start(va, format);
	linebuf_put_msg(linebuf, &strings);
	va_end(va);
}

/* build_msgbuf_tags
 *
 * inputs       - msgbuf object, client the message is from
 * outputs      - none
 * side effects - a msgbuf object is populated with an origin and relevant tags
 * notes        - to make this reentrant, find a solution for `buf` below
 */
static void
build_msgbuf_tags(struct MsgBuf *msgbuf, struct Client *from)
{
	hook_data hdata;

	msgbuf_init(msgbuf);

	hdata.client = from;
	hdata.arg1 = msgbuf;

	call_hook(h_outbound_msgbuf, &hdata);
}

/* sendto_one()
 *
 * inputs	- client to send to, va_args
 * outputs	- client has message put into its queue
 * side effects -
 */
void
sendto_one(struct Client *target_p, const char *pattern, ...)
{
	va_list args;
	struct MsgBuf msgbuf;
	buf_head_t linebuf;
	rb_strf_t strings = { .format = pattern, .format_args = &args, .next = NULL };

	/* send remote if to->from non NULL */
	if(target_p->from != NULL)
		target_p = target_p->from;

	if(IsIOError(target_p))
		return;

	rb_linebuf_newbuf(&linebuf);

	build_msgbuf_tags(&msgbuf, &me);
	va_start(args, pattern);
	linebuf_put_tags(&linebuf, &msgbuf, target_p, &strings);
	va_end(args);

	_send_linebuf(target_p, &linebuf);

	rb_linebuf_donebuf(&linebuf);
}

/* sendto_one_prefix()
 *
 * inputs	- client to send to, va_args
 * outputs	- client has message put into its queue
 * side effects - source(us)/target is chosen based on TS6 capability
 */
void
sendto_one_prefix(struct Client *target_p, struct Client *source_p,
		  const char *command, const char *pattern, ...)
{
	struct Client *dest_p = target_p->from;
	va_list args;
	struct MsgBuf msgbuf;
	buf_head_t linebuf;
	rb_strf_t strings = { .format = pattern, .format_args = &args, .next = NULL };

	if(IsIOError(dest_p))
		return;

	if(IsMe(dest_p))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "Trying to send to myself!");
		return;
	}

	build_msgbuf_tags(&msgbuf, source_p);

	rb_linebuf_newbuf(&linebuf);
	va_start(args, pattern);
	linebuf_put_tagsf(&linebuf, &msgbuf, target_p, &strings,
		":%s %s %s ", get_id(source_p, target_p),
		command, get_id(target_p, target_p));
	va_end(args);

	_send_linebuf(dest_p, &linebuf);
	rb_linebuf_donebuf(&linebuf);
}

/* sendto_one_notice()
 *
 * inputs	- client to send to, va_args
 * outputs	- client has a NOTICE put into its queue
 * side effects - source(us)/target is chosen based on TS6 capability
 */
void
sendto_one_notice(struct Client *target_p, const char *pattern, ...)
{
	struct Client *dest_p = target_p->from;
	va_list args;
	struct MsgBuf msgbuf;
	buf_head_t linebuf;
	rb_strf_t strings = { .format = pattern, .format_args = &args, .next = NULL };
	char *to;

	if(IsIOError(dest_p))
		return;

	if(IsMe(dest_p))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "Trying to send to myself!");
		return;
	}

	build_msgbuf_tags(&msgbuf, &me);

	rb_linebuf_newbuf(&linebuf);
	va_start(args, pattern);
	linebuf_put_tagsf(&linebuf, &msgbuf, target_p, &strings,
		":%s NOTICE %s ", get_id(&me, target_p),
		*(to = get_id(target_p, target_p)) != '\0' ? to : "*");
	va_end(args);

	_send_linebuf(dest_p, &linebuf);
	rb_linebuf_donebuf(&linebuf);
}


/* sendto_one_numeric()
 *
 * inputs	- client to send to, va_args
 * outputs	- client has message put into its queue
 * side effects - source/target is chosen based on TS6 capability
 */
void
sendto_one_numeric(struct Client *target_p, int numeric, const char *pattern, ...)
{
	struct Client *dest_p = target_p->from;
	va_list args;
	struct MsgBuf msgbuf;
	buf_head_t linebuf;
	rb_strf_t strings = { .format = pattern, .format_args = &args, .next = NULL };
	char *to;

	if(IsIOError(dest_p))
		return;

	if(IsMe(dest_p))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "Trying to send to myself!");
		return;
	}

	build_msgbuf_tags(&msgbuf, &me);

	rb_linebuf_newbuf(&linebuf);
	va_start(args, pattern);
	linebuf_put_tagsf(&linebuf, &msgbuf, target_p, &strings,
		":%s %03d %s ", get_id(&me, target_p), numeric,
		*(to = get_id(target_p, target_p)) != '\0' ? to : "*");
	va_end(args);

	_send_linebuf(dest_p, &linebuf);
	rb_linebuf_donebuf(&linebuf);
}

/*
 * sendto_server
 *
 * inputs       - pointer to client to NOT send to
 *              - caps or'd together which must ALL be present
 *              - caps or'd together which must ALL NOT be present
 *              - printf style format string
 *              - args to format string
 * output       - NONE
 * side effects - Send a message to all connected servers, except the
 *                client 'one' (if non-NULL), as long as the servers
 *                support ALL capabs in 'caps', and NO capabs in 'nocaps'.
 *
 * This function was written in an attempt to merge together the other
 * billion sendto_*serv*() functions, which sprung up with capabs, uids etc
 * -davidt
 */
void
sendto_server(struct Client *one, struct Channel *chptr, unsigned long caps,
	      unsigned long nocaps, const char *format, ...)
{
	va_list args;
	struct Client *target_p;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	buf_head_t linebuf;
	rb_strf_t strings = { .format = format, .format_args = &args, .next = NULL };

	/* noone to send to.. */
	if(rb_dlink_list_length(&serv_list) == 0)
		return;

	if(chptr != NULL && *chptr->chname != '#')
			return;

	rb_linebuf_newbuf(&linebuf);
	va_start(args, format);
	linebuf_put_msg(&linebuf, &strings);
	va_end(args);

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, serv_list.head)
	{
		target_p = ptr->data;

		/* check against 'one' */
		if(one != NULL && (target_p == one->from))
			continue;

		/* check we have required capabs */
		if(!IsCapable(target_p, caps))
			continue;

		/* check we don't have any forbidden capabs */
		if(!NotCapable(target_p, nocaps))
			continue;

		_send_linebuf(target_p, &linebuf);
	}

	rb_linebuf_donebuf(&linebuf);
}

/* sendto_channel_flags()
 *
 * inputs	- server not to send to, flags needed, source, channel, va_args
 * outputs	- message is sent to channel members
 * side effects -
 */
void
sendto_channel_flags(struct Client *one, int type, struct Client *source_p,
		     struct Channel *chptr, const char *pattern, ...)
{
	static char buf[BUFSIZE];
	va_list args;
	buf_head_t rb_linebuf_remote;
	struct Client *target_p;
	struct membership *msptr;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	struct MsgBuf msgbuf;
	struct MsgBuf_cache msgbuf_cache;
	rb_strf_t strings = { .format = buf, .format_args = NULL, .next = NULL };

	rb_linebuf_newbuf(&rb_linebuf_remote);

	current_serial++;

	build_msgbuf_tags(&msgbuf, source_p);

	va_start(args, pattern);
	vsnprintf(buf, sizeof buf, pattern, args);
	va_end(args);

	linebuf_put_msgf(&rb_linebuf_remote, NULL, ":%s %s", use_id(source_p), buf);
	msgbuf_cache_initf(&msgbuf_cache, &msgbuf, &strings,
		IsPerson(source_p) ? ":%1$s!%2$s@%3$s " : ":%1$s ",
		source_p->name, source_p->username, source_p->host);

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, chptr->members.head)
	{
		msptr = ptr->data;
		target_p = msptr->client_p;

		if(!MyClient(source_p) && (IsIOError(target_p->from) || target_p->from == one))
			continue;

		if(MyClient(source_p) && target_p == one)
			continue;

		if(type && ((msptr->flags & type) == 0))
			continue;

		if(IsDeaf(target_p))
			continue;

		if(!MyClient(target_p))
		{
			/* if we've got a specific type, target must support
			 * CHW.. --fl
			 */
			if(type && NotCapable(target_p->from, CAP_CHW))
				continue;

			if(target_p->from->serial != current_serial)
			{
				send_linebuf_remote(target_p, source_p, &rb_linebuf_remote);
				target_p->from->serial = current_serial;
			}
		}
		else
		{
			_send_linebuf(target_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(target_p)));
		}
	}

	/* source client may not be on the channel, send echo separately */
	if(MyClient(source_p) && IsCapable(source_p, CLICAP_ECHO_MESSAGE))
	{
		target_p = one;

		_send_linebuf(target_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(target_p)));
	}

	rb_linebuf_donebuf(&rb_linebuf_remote);
	msgbuf_cache_free(&msgbuf_cache);
}

/* sendto_channel_flags()
 *
 * inputs	- server not to send to, flags needed, source, channel, va_args
 * outputs	- message is sent to channel members
 * side effects -
 */
void
sendto_channel_opmod(struct Client *one, struct Client *source_p,
		     struct Channel *chptr, const char *command,
		     const char *text)
{
	buf_head_t rb_linebuf_old;
	buf_head_t rb_linebuf_new;
	struct Client *target_p;
	struct membership *msptr;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	struct MsgBuf msgbuf;
	struct MsgBuf_cache msgbuf_cache;
	rb_strf_t strings = { .format = text, .format_args = NULL, .next = NULL };

	rb_linebuf_newbuf(&rb_linebuf_old);
	rb_linebuf_newbuf(&rb_linebuf_new);

	build_msgbuf_tags(&msgbuf, source_p);

	current_serial++;
	const char *statusmsg_prefix = (ConfigChannel.opmod_send_statusmsg ? "@" : "");

	if(IsServer(source_p)) {
		msgbuf_cache_initf(&msgbuf_cache, &msgbuf, &strings,
			       ":%s %s %s%s :",
			       source_p->name, command, statusmsg_prefix, chptr->chname);
	} else {
		msgbuf_cache_initf(&msgbuf_cache, &msgbuf, &strings,
			       ":%s!%s@%s %s %s%s :",
			       source_p->name, source_p->username,
			       source_p->host, command, statusmsg_prefix, chptr->chname);
	}

	if (chptr->mode.mode & MODE_MODERATED) {
		linebuf_put_msgf(&rb_linebuf_old, &strings,
			       ":%s %s %s%s :",
			       use_id(source_p), command, statusmsg_prefix, chptr->chname, text);
	} else {
		linebuf_put_msgf(&rb_linebuf_old, &strings,
			       ":%s NOTICE @%s :<%s:%s> ",
			       use_id(source_p->servptr), chptr->chname,
			       source_p->name, chptr->chname);
	}
	linebuf_put_msgf(&rb_linebuf_new, &strings,
		       ":%s %s =%s :",
		       use_id(source_p), command, chptr->chname);
	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, chptr->members.head)
	{
		msptr = ptr->data;
		target_p = msptr->client_p;

		if(!MyClient(source_p) && (IsIOError(target_p->from) || target_p->from == one))
			continue;

		if(MyClient(source_p) && target_p == one)
			continue;

		if((msptr->flags & CHFL_CHANOP) == 0)
			continue;

		if(IsDeaf(target_p))
			continue;

		if(!MyClient(target_p))
		{
			/* if we've got a specific type, target must support
			 * CHW.. --fl
			 */
			if(NotCapable(target_p->from, CAP_CHW))
				continue;

			if(target_p->from->serial != current_serial)
			{
				if (IsCapable(target_p->from, CAP_EOPMOD))
					send_linebuf_remote(target_p, source_p, &rb_linebuf_new);
				else
					send_linebuf_remote(target_p, source_p, &rb_linebuf_old);
				target_p->from->serial = current_serial;
			}
		} else {
			_send_linebuf(target_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(target_p)));
		}
	}

	/* source client may not be on the channel, send echo separately */
	if(MyClient(source_p) && IsCapable(source_p, CLICAP_ECHO_MESSAGE))
	{
		target_p = one;

		_send_linebuf(target_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(target_p)));
	}

	rb_linebuf_donebuf(&rb_linebuf_old);
	rb_linebuf_donebuf(&rb_linebuf_new);
	msgbuf_cache_free(&msgbuf_cache);
}

/* _sendto_channel_local
 *
 * inputs	- source, flags to send to, privs to send to, channel to send to, va_args
 * outputs	- message to local channel members
 * side effects -
 */
void
_sendto_channel_local(struct Client *source_p, int type, const char *priv, struct Channel *chptr, const char *pattern, va_list *args)
{
	struct membership *msptr;
	struct Client *target_p;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	struct MsgBuf msgbuf;
	struct MsgBuf_cache msgbuf_cache;
	rb_strf_t strings = { .format = pattern, .format_args = args, .next = NULL };

	build_msgbuf_tags(&msgbuf, source_p);

	msgbuf_cache_init(&msgbuf_cache, &msgbuf, &strings);

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, chptr->locmembers.head)
	{
		msptr = ptr->data;
		target_p = msptr->client_p;

		if (IsIOError(target_p))
			continue;

		if (type && ((msptr->flags & type) == 0))
			continue;

		if (priv != NULL && !HasPrivilege(target_p, priv))
			continue;

		_send_linebuf(target_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(target_p)));
	}

	msgbuf_cache_free(&msgbuf_cache);
}

/* sendto_channel_local_priv()
 *
 * inputs	- source, flags to send to, privs to send to, channel to send to, va_args
 * outputs	- message to local channel members
 * side effects -
 */
void
sendto_channel_local_priv(struct Client *source_p, int type, const char *priv, struct Channel *chptr, const char *pattern, ...)
{
	va_list args;
	va_start(args, pattern);
	_sendto_channel_local(source_p, type, priv, chptr, pattern, &args);
	va_end(args);
}

/* sendto_channel_local()
 *
 * inputs	- source, flags to send to, channel to send to, va_args
 * outputs	- message to local channel members
 * side effects -
 */
void
sendto_channel_local(struct Client *source_p, int type, struct Channel *chptr, const char *pattern, ...)
{
	va_list args;
	va_start(args, pattern);
	_sendto_channel_local(source_p, type, NULL, chptr, pattern, &args);
	va_end(args);
}

/*
 * _sendto_channel_local_with_capability_butone()
 *
 * Shared implementation of sendto_channel_local_with_capability and sendto_channel_local_with_capability_butone
 */
static void
_sendto_channel_local_with_capability_butone(struct Client *source_p, struct Client *one, int type,
	int caps, int negcaps, struct Channel *chptr, const char *pattern, va_list * args)
{
	struct membership *msptr;
	struct Client *target_p;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	struct MsgBuf msgbuf;
	struct MsgBuf_cache msgbuf_cache;
	rb_strf_t strings = { .format = pattern, .format_args = args, .next = NULL };

	build_msgbuf_tags(&msgbuf, source_p);
	msgbuf_cache_init(&msgbuf_cache, &msgbuf, &strings);

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, chptr->locmembers.head)
	{
		msptr = ptr->data;
		target_p = msptr->client_p;

		if (target_p == one)
			continue;

		if(IsIOError(target_p) ||
		   !IsCapable(target_p, caps) ||
 		   !NotCapable(target_p, negcaps))
			continue;

		if(type && ((msptr->flags & type) == 0))
			continue;

		_send_linebuf(target_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(target_p)));
	}

	msgbuf_cache_free(&msgbuf_cache);
}

/* sendto_channel_local_with_capability()
 *
 * inputs	- source, flags to send to, caps, negate caps, channel to send to, va_args
 * outputs	- message to local channel members
 * side effects -
 */
void
sendto_channel_local_with_capability(struct Client *source_p, int type, int caps, int negcaps, struct Channel *chptr, const char *pattern, ...)
{
	va_list args;

	va_start(args, pattern);
	_sendto_channel_local_with_capability_butone(source_p, NULL, type, caps, negcaps, chptr, pattern, &args);
	va_end(args);
}


/* sendto_channel_local_with_capability()
 *
 * inputs	- source, flags to send to, caps, negate caps, channel to send to, va_args
 * outputs	- message to local channel members
 * side effects -
 */
void
sendto_channel_local_with_capability_butone(struct Client *one, int type,
	int caps, int negcaps, struct Channel *chptr, const char *pattern, ...)
{
	va_list args;

	va_start(args, pattern);
	_sendto_channel_local_with_capability_butone(one, one, type, caps, negcaps, chptr, pattern, &args);
	va_end(args);
}


/* sendto_channel_local_butone()
 *
 * inputs	- flags to send to, channel to send to, va_args
 *		- user to ignore when sending
 * outputs	- message to local channel members
 * side effects -
 */
void
sendto_channel_local_butone(struct Client *one, int type, struct Channel *chptr, const char *pattern, ...)
{
	va_list args;
	struct membership *msptr;
	struct Client *target_p;
	struct MsgBuf msgbuf;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	struct MsgBuf_cache msgbuf_cache;
	rb_strf_t strings = { .format = pattern, .format_args = &args, .next = NULL };

	build_msgbuf_tags(&msgbuf, one);

	va_start(args, pattern);
	msgbuf_cache_init(&msgbuf_cache, &msgbuf, &strings);
	va_end(args);

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, chptr->locmembers.head)
	{
		msptr = ptr->data;
		target_p = msptr->client_p;

		if(target_p == one)
			continue;

		if(IsIOError(target_p))
			continue;

		if(type && ((msptr->flags & type) == 0))
			continue;

		/* attach the present linebuf to the target */
		_send_linebuf(target_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(target_p)));
	}

	msgbuf_cache_free(&msgbuf_cache);
}

/*
 * sendto_common_channels_local()
 *
 * inputs	- pointer to client
 *		- capability mask
 *		- negated capability mask
 *		- pattern to send
 * output	- NONE
 * side effects	- Sends a message to all people on local server who are
 * 		  in same channel with user.
 *		  used by m_nick.c and exit_one_client.
 */
void
sendto_common_channels_local(struct Client *user, int cap, int negcap, const char *pattern, ...)
{
	va_list args;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	rb_dlink_node *uptr;
	rb_dlink_node *next_uptr;
	struct Channel *chptr;
	struct Client *target_p;
	struct membership *msptr;
	struct membership *mscptr;
	struct MsgBuf msgbuf;
	struct MsgBuf_cache msgbuf_cache;
	rb_strf_t strings = { .format = pattern, .format_args = &args, .next = NULL };

	build_msgbuf_tags(&msgbuf, user);

	va_start(args, pattern);
	msgbuf_cache_init(&msgbuf_cache, &msgbuf, &strings);
	va_end(args);

	++current_serial;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, user->user->channel.head)
	{
		mscptr = ptr->data;
		chptr = mscptr->chptr;

		RB_DLINK_FOREACH_SAFE(uptr, next_uptr, chptr->locmembers.head)
		{
			msptr = uptr->data;
			target_p = msptr->client_p;

			if(IsIOError(target_p) ||
			   target_p->serial == current_serial ||
			   !IsCapable(target_p, cap) ||
			   !NotCapable(target_p, negcap))
				continue;

			target_p->serial = current_serial;
			send_linebuf(target_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(target_p)));
		}
	}

	/* this can happen when the user isnt in any channels, but we still
	 * need to send them the data, ie a nick change
	 */
	if(MyConnect(user) && (user->serial != current_serial)
			&& IsCapable(user, cap) && NotCapable(user, negcap)) {
		send_linebuf(user, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(user)));
	}

	msgbuf_cache_free(&msgbuf_cache);
}

/*
 * sendto_common_channels_local_butone()
 *
 * inputs	- pointer to client
 *		- capability mask
 *		- negated capability mask
 *		- pattern to send
 * output	- NONE
 * side effects	- Sends a message to all people on local server who are
 * 		  in same channel with user, except for user itself.
 */
void
sendto_common_channels_local_butone(struct Client *user, int cap, int negcap, const char *pattern, ...)
{
	va_list args;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	rb_dlink_node *uptr;
	rb_dlink_node *next_uptr;
	struct Channel *chptr;
	struct Client *target_p;
	struct membership *msptr;
	struct membership *mscptr;
	struct MsgBuf msgbuf;
	struct MsgBuf_cache msgbuf_cache;
	rb_strf_t strings = { .format = pattern, .format_args = &args, .next = NULL };

	build_msgbuf_tags(&msgbuf, user);

	va_start(args, pattern);
	msgbuf_cache_init(&msgbuf_cache, &msgbuf, &strings);
	va_end(args);

	++current_serial;
	/* Skip them -- jilles */
	user->serial = current_serial;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, user->user->channel.head)
	{
		mscptr = ptr->data;
		chptr = mscptr->chptr;

		RB_DLINK_FOREACH_SAFE(uptr, next_uptr, chptr->locmembers.head)
		{
			msptr = uptr->data;
			target_p = msptr->client_p;

			if(IsIOError(target_p) ||
			   target_p->serial == current_serial ||
			   !IsCapable(target_p, cap) ||
			   !NotCapable(target_p, negcap))
				continue;

			target_p->serial = current_serial;
			send_linebuf(target_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(target_p)));
		}
	}

	msgbuf_cache_free(&msgbuf_cache);
}

/* sendto_match_butone()
 *
 * inputs	- server not to send to, source, mask, type of mask, va_args
 * output	-
 * side effects - message is sent to matching clients
 */
void
sendto_match_butone(struct Client *one, struct Client *source_p,
		    const char *mask, int what, const char *pattern, ...)
{
	static char buf[BUFSIZE];
	va_list args;
	struct Client *target_p;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	buf_head_t rb_linebuf_remote;
	struct MsgBuf msgbuf;
	struct MsgBuf_cache msgbuf_cache;
	rb_strf_t strings = { .format = buf, .format_args = NULL, .next = NULL };

	rb_linebuf_newbuf(&rb_linebuf_remote);

	build_msgbuf_tags(&msgbuf, source_p);

	va_start(args, pattern);
	vsnprintf(buf, sizeof(buf), pattern, args);
	va_end(args);

	msgbuf_cache_initf(&msgbuf_cache, &msgbuf, &strings,
		IsServer(source_p) ? ":%s " : ":%s!%s@%s ",
		source_p->name, source_p->username, source_p->host);

	linebuf_put_msgf(&rb_linebuf_remote, &strings, ":%s ", use_id(source_p));

	if(what == MATCH_HOST)
	{
		RB_DLINK_FOREACH_SAFE(ptr, next_ptr, lclient_list.head)
		{
			target_p = ptr->data;

			if(match(mask, target_p->host))
				_send_linebuf(target_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(target_p)));
		}
	}
	/* what = MATCH_SERVER, if it doesnt match us, just send remote */
	else if(match(mask, me.name))
	{
		RB_DLINK_FOREACH_SAFE(ptr, next_ptr, lclient_list.head)
		{
			target_p = ptr->data;
			_send_linebuf(target_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(target_p)));
		}
	}

	RB_DLINK_FOREACH(ptr, serv_list.head)
	{
		target_p = ptr->data;

		if(target_p == one)
			continue;

		send_linebuf_remote(target_p, source_p, &rb_linebuf_remote);
	}

	msgbuf_cache_free(&msgbuf_cache);
	rb_linebuf_donebuf(&rb_linebuf_remote);
}

/* sendto_match_servs()
 *
 * inputs       - source, mask to send to, caps needed, va_args
 * outputs      -
 * side effects - message is sent to matching servers with caps.
 */
void
sendto_match_servs(struct Client *source_p, const char *mask, int cap,
			int nocap, const char *pattern, ...)
{
	static char buf[BUFSIZE];
	va_list args;
	rb_dlink_node *ptr;
	struct Client *target_p;
	buf_head_t rb_linebuf_id;
	rb_strf_t strings = { .format = buf, .format_args = NULL, .next = NULL };

	if(EmptyString(mask))
		return;

	rb_linebuf_newbuf(&rb_linebuf_id);

	va_start(args, pattern);
	vsnprintf(buf, sizeof(buf), pattern, args);
	va_end(args);

	linebuf_put_msgf(&rb_linebuf_id, &strings, ":%s ", use_id(source_p));

	current_serial++;

	RB_DLINK_FOREACH(ptr, global_serv_list.head)
	{
		target_p = ptr->data;

		/* dont send to ourselves, or back to where it came from.. */
		if(IsMe(target_p) || target_p->from == source_p->from)
			continue;

		if(target_p->from->serial == current_serial)
			continue;

		if(match(mask, target_p->name))
		{
			/* if we set the serial here, then we'll never do
			 * a match() again if !IsCapable()
			 */
			target_p->from->serial = current_serial;

			if(cap && !IsCapable(target_p->from, cap))
				continue;

			if(nocap && !NotCapable(target_p->from, nocap))
				continue;

			_send_linebuf(target_p->from, &rb_linebuf_id);
		}
	}

	rb_linebuf_donebuf(&rb_linebuf_id);
}

/* sendto_local_clients_with_capability()
 *
 * inputs       - caps needed, pattern, va_args
 * outputs      -
 * side effects - message is sent to matching local clients with caps.
 */
void
sendto_local_clients_with_capability(int cap, const char *pattern, ...)
{
	va_list args;
	rb_dlink_node *ptr;
	struct Client *target_p;
	struct MsgBuf msgbuf;
	struct MsgBuf_cache msgbuf_cache;
	rb_strf_t strings = { .format = pattern, .format_args = &args, .next = NULL };

	build_msgbuf_tags(&msgbuf, &me);

	va_start(args, pattern);
	msgbuf_cache_init(&msgbuf_cache, &msgbuf, &strings);
	va_end(args);

	RB_DLINK_FOREACH(ptr, lclient_list.head)
	{
		target_p = ptr->data;

		if(IsIOError(target_p) || !IsCapable(target_p, cap))
			continue;

		send_linebuf(target_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(target_p)));
	}

	msgbuf_cache_free(&msgbuf_cache);
}

/* sendto_monitor()
 *
 * inputs	- monitor nick to send to, format, va_args
 * outputs	- message to local users monitoring the given nick
 * side effects -
 */
void
sendto_monitor(struct Client *source_p, struct monitor *monptr, const char *pattern, ...)
{
	va_list args;
	struct Client *target_p;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	struct MsgBuf msgbuf;
	struct MsgBuf_cache msgbuf_cache;
	rb_strf_t strings = { .format = pattern, .format_args = &args, .next = NULL };

	build_msgbuf_tags(&msgbuf, source_p);

	va_start(args, pattern);
	msgbuf_cache_init(&msgbuf_cache, &msgbuf, &strings);
	va_end(args);

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, monptr->users.head)
	{
		target_p = ptr->data;

		if(IsIOError(target_p))
			continue;

		_send_linebuf(target_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(target_p)));
	}

	msgbuf_cache_free(&msgbuf_cache);
}

/* _sendto_anywhere()
 *
 * inputs	- real_target, target, source, va_args
 * outputs	-
 * side effects - client is sent message/own message with correct prefix.
 */
static void
_sendto_anywhere(struct Client *dest_p, struct Client *target_p,
		struct Client *source_p, const char *command,
		const char *pattern, va_list *args)
{
	buf_head_t linebuf;
	rb_strf_t strings = { .format = pattern, .format_args = args, .next = NULL };

	rb_linebuf_newbuf(&linebuf);

	if (MyClient(dest_p)) {
		if (IsServer(source_p)) {
			linebuf_put_msgf(&linebuf, &strings, ":%s %s %s ",
				       source_p->name, command,
				       target_p->name);
		} else {
			struct MsgBuf msgbuf;

			build_msgbuf_tags(&msgbuf, source_p);

			linebuf_put_tagsf(&linebuf, &msgbuf, dest_p, &strings,
				IsPerson(source_p) ? ":%1$s!%4$s@%5$s %2$s %3$s " : ":%1$s %2$s %3$s ",
				source_p->name, command, target_p->name,
				source_p->username, source_p->host);
		}

		_send_linebuf(dest_p, &linebuf);
	} else {
		linebuf_put_msgf(&linebuf, &strings, ":%s %s %s ",
			       get_id(source_p, target_p), command,
			       get_id(target_p, target_p));

		send_linebuf_remote(dest_p, source_p, &linebuf);
	}

	rb_linebuf_donebuf(&linebuf);
}

/* sendto_anywhere()
 *
 * inputs	- target, source, va_args
 * outputs	-
 * side effects - client is sent message with correct prefix.
 */
void
sendto_anywhere(struct Client *target_p, struct Client *source_p,
		const char *command, const char *pattern, ...)
{
	va_list args;

	va_start(args, pattern);
	_sendto_anywhere(target_p, target_p, source_p, command, pattern, &args);
	va_end(args);
}

/* sendto_anywhere_echo()
 *
 * inputs	- target, source, va_args
 * outputs	-
 * side effects - client is sent own message with correct prefix.
 */
void
sendto_anywhere_echo(struct Client *target_p, struct Client *source_p,
		const char *command, const char *pattern, ...)
{
	va_list args;

	s_assert(MyClient(source_p));
	s_assert(!IsServer(source_p));

	va_start(args, pattern);
	_sendto_anywhere(source_p, target_p, source_p, command, pattern, &args);
	va_end(args);
}

/* sendto_realops_snomask()
 *
 * inputs	- snomask needed, level (opers/admin), va_args
 * output	-
 * side effects - message is sent to opers with matching snomasks
 */
void
sendto_realops_snomask(int flags, int level, const char *pattern, ...)
{
	static char buf[BUFSIZE];
	char *snobuf;
	struct Client *client_p;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	va_list args;
	struct MsgBuf msgbuf;
	struct MsgBuf_cache msgbuf_cache;

	build_msgbuf_tags(&msgbuf, &me);

	/* rather a lot of copying around, oh well -- jilles */
	va_start(args, pattern);
	vsnprintf(buf, sizeof(buf), pattern, args);
	va_end(args);

	msgbuf_cache_initf(&msgbuf_cache, &msgbuf, NULL,
		":%s NOTICE * :*** Notice -- %s", me.name, buf);

	/* Be very sure not to do things like "Trying to send to myself"
	 * L_NETWIDE, otherwise infinite recursion may result! -- jilles */
	if (level & L_NETWIDE && ConfigFileEntry.global_snotices)
	{
		snobuf = construct_snobuf(flags);
		if (snobuf[1] != '\0')
			sendto_server(NULL, NULL, CAP_ENCAP|CAP_TS6, NOCAPS,
					":%s ENCAP * SNOTE %c :%s",
					me.id, snobuf[1], buf);
	}
	else if (remote_rehash_oper_p != NULL)
	{
		sendto_one_notice(remote_rehash_oper_p, ":*** Notice -- %s", buf);
	}
	level &= ~L_NETWIDE;

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, local_oper_list.head)
	{
		client_p = ptr->data;

		/* If we're sending it to opers and theyre an admin, skip.
		 * If we're sending it to admins, and theyre not, skip.
		 */
		if(((level == L_ADMIN) && !IsOperAdmin(client_p)) ||
		   ((level == L_OPER) && IsOperAdmin(client_p)))
			continue;

		if (client_p->snomask & flags) {
			_send_linebuf(client_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(client_p)));
		}
	}

	msgbuf_cache_free(&msgbuf_cache);
}
/* sendto_realops_snomask_from()
 *
 * inputs	- snomask needed, level (opers/admin), source server, va_args
 * output	-
 * side effects - message is sent to opers with matching snomask
 */
void
sendto_realops_snomask_from(int flags, int level, struct Client *source_p,
		const char *pattern, ...)
{
	struct Client *client_p;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	va_list args;
	struct MsgBuf msgbuf;
	struct MsgBuf_cache msgbuf_cache;
	rb_strf_t strings = { .format = pattern, .format_args = &args, .next = NULL };

	build_msgbuf_tags(&msgbuf, &me);

	va_start(args, pattern);
	msgbuf_cache_initf(&msgbuf_cache, &msgbuf, &strings,
		       ":%s NOTICE * :*** Notice -- ", source_p->name);
	va_end(args);

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, local_oper_list.head)
	{
		client_p = ptr->data;

		/* If we're sending it to opers and theyre an admin, skip.
		 * If we're sending it to admins, and theyre not, skip.
		 */
		if(((level == L_ADMIN) && !IsOperAdmin(client_p)) ||
		   ((level == L_OPER) && IsOperAdmin(client_p)))
			continue;

		if (client_p->snomask & flags) {
			_send_linebuf(client_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(client_p)));
		}
	}

	msgbuf_cache_free(&msgbuf_cache);
}

/*
 * sendto_wallops_flags
 *
 * inputs       - flag types of messages to show to real opers
 *              - client sending request
 *              - var args input message
 * output       - NONE
 * side effects - Send a wallops to local opers
 */
void
sendto_wallops_flags(int flags, struct Client *source_p, const char *pattern, ...)
{
	struct Client *client_p;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	va_list args;
	struct MsgBuf msgbuf;
	struct MsgBuf_cache msgbuf_cache;
	rb_strf_t strings = { .format = pattern, .format_args = &args, .next = NULL };

	build_msgbuf_tags(&msgbuf, source_p);

	va_start(args, pattern);
	if (IsPerson(source_p)) {
		msgbuf_cache_initf(&msgbuf_cache, &msgbuf, &strings,
			       ":%s!%s@%s WALLOPS :", source_p->name,
			       source_p->username, source_p->host);
	} else {
		msgbuf_cache_initf(&msgbuf_cache, &msgbuf, &strings,
			":%s WALLOPS :", source_p->name);
	}
	va_end(args);

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, IsPerson(source_p) && flags == UMODE_WALLOP ? lclient_list.head : local_oper_list.head)
	{
		client_p = ptr->data;

		if (client_p->umodes & flags) {
			_send_linebuf(client_p, msgbuf_cache_get(&msgbuf_cache, CLIENT_CAP_MASK(client_p)));
		}
	}

	msgbuf_cache_free(&msgbuf_cache);
}

/* kill_client()
 *
 * input	- client to send kill to, client to kill, va_args
 * output	-
 * side effects - we issue a kill for the client
 */
void
kill_client(struct Client *target_p, struct Client *diedie, const char *pattern, ...)
{
	va_list args;
	buf_head_t linebuf;
	struct MsgBuf msgbuf;
	rb_strf_t strings = { .format = pattern, .format_args = &args, .next = NULL };

	build_msgbuf_tags(&msgbuf, &me);

	rb_linebuf_newbuf(&linebuf);

	va_start(args, pattern);
	linebuf_put_tagsf(&linebuf, &msgbuf, target_p, &strings,
		":%s KILL %s :", get_id(&me, target_p), get_id(diedie, target_p));
	va_end(args);

	send_linebuf(target_p, &linebuf);
	rb_linebuf_donebuf(&linebuf);
}


/*
 * kill_client_serv_butone
 *
 * inputs	- pointer to client to not send to
 *		- pointer to client to kill
 * output	- NONE
 * side effects	- Send a KILL for the given client
 *		  message to all connected servers
 *                except the client 'one'. Also deal with
 *		  client being unknown to leaf, as in lazylink...
 */
void
kill_client_serv_butone(struct Client *one, struct Client *target_p, const char *pattern, ...)
{
	static char buf[BUFSIZE];
	va_list args;
	struct Client *client_p;
	rb_dlink_node *ptr;
	rb_dlink_node *next_ptr;
	buf_head_t rb_linebuf_id;
	rb_strf_t strings = { .format = pattern, .format_args = &args, .next = NULL };

	rb_linebuf_newbuf(&rb_linebuf_id);

	va_start(args, pattern);
	linebuf_put_msgf(&rb_linebuf_id, &strings, ":%s KILL %s :%s",
		       use_id(&me), use_id(target_p), buf);
	va_end(args);

	RB_DLINK_FOREACH_SAFE(ptr, next_ptr, serv_list.head)
	{
		client_p = ptr->data;

		/* ok, if the client we're supposed to not send to has an
		 * ID, then we still want to issue the kill there..
		 */
		if(one != NULL && (client_p == one->from) &&
			(!has_id(client_p) || !has_id(target_p)))
			continue;

		_send_linebuf(client_p, &rb_linebuf_id);
	}

	rb_linebuf_donebuf(&rb_linebuf_id);
}

static struct Client *multiline_stashed_target_p;
static char multiline_prefix[DATALEN+1]; /* allow for null termination */
static int multiline_prefix_len;
static char multiline_separator[2];
static int multiline_separator_len;
static char *multiline_item_start;
static char *multiline_cur;
static int multiline_cur_len;
static int multiline_remote_pad;

bool
send_multiline_init(struct Client *target_p, const char *separator, const char *format, ...)
{
	va_list args;

	s_assert(multiline_stashed_target_p == NULL && "Multiline: didn't cleanup after last usage!");

	va_start(args, format);
	multiline_prefix_len = vsnprintf(multiline_prefix, sizeof multiline_prefix, format, args);
	va_end(args);

	if (multiline_prefix_len <= 0 || multiline_prefix_len >= DATALEN)
	{
		s_assert(false && "Multiline: failure preparing prefix!");
		return false;
	}

	multiline_separator_len = rb_strlcpy(multiline_separator, separator, sizeof multiline_separator);
	if (multiline_separator_len >= sizeof multiline_separator)
	{
		s_assert(false && "Multiline: separator too long");
		return false;
	}

	multiline_stashed_target_p = target_p;
	multiline_item_start = multiline_prefix + multiline_prefix_len;
	multiline_cur = multiline_item_start;
	multiline_cur_len = multiline_prefix_len;
	multiline_remote_pad = 0;
	return true;
}

bool
send_multiline_remote_pad(struct Client *target_p, struct Client *client_p)
{
	ssize_t remote_pad;

	if (target_p != multiline_stashed_target_p)
	{
		s_assert(false && "Multiline: missed init call!");
		multiline_stashed_target_p = NULL;
		return false;
	}

	if (MyConnect(target_p))
		return true;

	remote_pad = strlen(client_p->name) - strlen(client_p->id);

	if (remote_pad > 0)
	{
		multiline_remote_pad += remote_pad;
	}

	return true;
}

enum multiline_item_result
send_multiline_item(struct Client *target_p, const char *format, ...)
{
	va_list args;
	char item[DATALEN];
	int item_len, res;
	enum multiline_item_result ret = MULTILINE_SUCCESS;

	if (target_p != multiline_stashed_target_p)
	{
		s_assert(false && "Multiline: missed init call!");
		multiline_stashed_target_p = NULL;
		return MULTILINE_FAILURE;
	}

	va_start(args, format);
	item_len = vsnprintf(item, sizeof item, format, args);
	va_end(args);

	if (item_len < 0 || multiline_prefix_len + multiline_remote_pad + item_len > DATALEN)
	{
		s_assert(false && "Multiline: failure preparing item!");
		multiline_stashed_target_p = NULL;
		return MULTILINE_FAILURE;
	}

	if (multiline_cur_len + ((*multiline_item_start != '\0') ? multiline_separator_len : 0) + item_len > DATALEN - multiline_remote_pad)
	{
		sendto_one(target_p, "%s", multiline_prefix);
		*multiline_item_start = '\0';
		multiline_cur_len = multiline_prefix_len;
		multiline_cur = multiline_item_start;
		ret = MULTILINE_WRAPPED;
	}

	res = snprintf(multiline_cur, sizeof multiline_prefix - multiline_cur_len, "%s%s",
			(*multiline_item_start != '\0') ? multiline_separator : "",
			item);

	if (res < 0)
	{
		s_assert(false && "Multiline: failure appending item!");
		multiline_stashed_target_p = NULL;
		return MULTILINE_FAILURE;
	}

	multiline_cur_len += res;
	multiline_cur += res;
	return ret;
}

bool
send_multiline_fini(struct Client *target_p, const char *format, ...)
{
	va_list args;
	char final[DATALEN];
	int final_len;

	if (target_p != multiline_stashed_target_p)
	{
		s_assert(false && "Multiline: missed init call!");
		multiline_stashed_target_p = NULL;
		return false;
	}

	if (multiline_cur_len == multiline_prefix_len)
	{
		multiline_stashed_target_p = NULL;
		return true;
	}

	if (format)
	{
		va_start(args, format);
		final_len = vsnprintf(final, sizeof final, format, args);
		va_end(args);

		if (final_len <= 0 || final_len > multiline_prefix_len)
		{
			s_assert(false && "Multiline: failure preparing final prefix!");
			multiline_stashed_target_p = NULL;
			return false;
		}
	}
	else
	{
		rb_strlcpy(final, multiline_prefix, multiline_prefix_len + 1);
	}

	sendto_one(target_p, "%s%s", final, multiline_item_start);

	multiline_stashed_target_p = NULL;
	return true;
}

void
send_multiline_reset(void)
{
	multiline_stashed_target_p = NULL;
}
