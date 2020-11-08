/* modules/m_monitor.c
 *
 *  Copyright (C) 2005 Lee Hardy <lee@leeh.co.uk>
 *  Copyright (C) 2005 ircd-ratbox development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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
#include "client.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "monitor.h"
#include "numeric.h"
#include "s_conf.h"
#include "send.h"
#include "supported.h"

static const char monitor_desc[] = "Provides the MONITOR facility for tracking user signon and signoff";

static int monitor_init(void);
static void monitor_deinit(void);
static void m_monitor(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message monitor_msgtab = {
	"MONITOR", 0, 0, 0, 0,
	{mg_unreg, {m_monitor, 2}, mg_ignore, mg_ignore, mg_ignore, {m_monitor, 2}}
};

mapi_clist_av1 monitor_clist[] = { &monitor_msgtab, NULL };

DECLARE_MODULE_AV2(monitor, monitor_init, monitor_deinit, monitor_clist, NULL, NULL, NULL, NULL, monitor_desc);

static int monitor_init(void)
{
	add_isupport("MONITOR", isupport_intptr, &ConfigFileEntry.max_monitor);
	return 0;
}

static void monitor_deinit(void)
{
	delete_isupport("MONITOR");
}

static void
add_monitor(struct Client *client_p, const char *nicks)
{
	rb_dlink_node *prev_head, *ptr;
	struct Client *target_p;
	struct monitor *monptr;
	const char *name;
	char *tmp;
	char *p;

	prev_head = client_p->localClient->monitor_list.head;
	tmp = LOCAL_COPY(nicks);

	for(name = rb_strtok_r(tmp, ",", &p); name; name = rb_strtok_r(NULL, ",", &p))
	{
		if(EmptyString(name) || strlen(name) > NICKLEN-1)
			continue;

		if(rb_dlink_list_length(&client_p->localClient->monitor_list) >=
			(unsigned long)ConfigFileEntry.max_monitor)
		{
			break;
		}

		if (!clean_nick(name, 0))
			continue;

		monptr = find_monitor(name, 1);

		/* already monitoring this nick */
		if(rb_dlinkFind(client_p, &monptr->users))
			continue;

		rb_dlinkAddAlloc(client_p, &monptr->users);
		rb_dlinkAddAlloc(monptr, &client_p->localClient->monitor_list);
	}

	send_multiline_init(client_p, ",", form_str(RPL_MONONLINE),
			me.name,
			client_p->name,
			"");

	if (prev_head != NULL)
		prev_head = prev_head->prev;
	else
		prev_head = client_p->localClient->monitor_list.tail;

	RB_DLINK_FOREACH_PREV(ptr, prev_head)
	{
		monptr = ptr->data;
		target_p = find_named_person(monptr->name);

		if (target_p != NULL)
		{
			send_multiline_item(client_p, "%s!%s@%s",
					target_p->name,
					target_p->username,
					target_p->host);
		}
	}

	send_multiline_fini(client_p, NULL);

	send_multiline_init(client_p, ",", form_str(RPL_MONOFFLINE),
			me.name,
			client_p->name,
			"");

	RB_DLINK_FOREACH_PREV(ptr, prev_head)
	{
		monptr = ptr->data;

		if (find_named_person(monptr->name) == NULL)
		{
			send_multiline_item(client_p, "%s", monptr->name);
		}
	}

	send_multiline_fini(client_p, NULL);

	if (name)
	{
		char buf[400];

		if (p)
			snprintf(buf, sizeof buf, "%s,%s", name, p);
		else
			snprintf(buf, sizeof buf, "%s", name);

		sendto_one(client_p, form_str(ERR_MONLISTFULL),
				me.name,
				client_p->name,
				ConfigFileEntry.max_monitor,
				buf);
	}
}

static void
del_monitor(struct Client *client_p, const char *nicks)
{
	struct monitor *monptr;
	const char *name;
	char *tmp;
	char *p;

	if(!rb_dlink_list_length(&client_p->localClient->monitor_list))
		return;

	tmp = LOCAL_COPY(nicks);

	for(name = rb_strtok_r(tmp, ",", &p); name; name = rb_strtok_r(NULL, ",", &p))
	{
		if(EmptyString(name))
			continue;

		/* not monitored */
		if((monptr = find_monitor(name, 0)) == NULL)
			continue;

		rb_dlinkFindDestroy(client_p, &monptr->users);
		rb_dlinkFindDestroy(monptr, &client_p->localClient->monitor_list);

		free_monitor(monptr);
	}
}

static void
list_monitor(struct Client *client_p)
{
	struct monitor *monptr;
	rb_dlink_node *ptr;

	if(!rb_dlink_list_length(&client_p->localClient->monitor_list))
	{
		sendto_one(client_p, form_str(RPL_ENDOFMONLIST),
				me.name, client_p->name);
		return;
	}

	send_multiline_init(client_p, ",", form_str(RPL_MONLIST),
			me.name,
			client_p->name,
			"");

	RB_DLINK_FOREACH(ptr, client_p->localClient->monitor_list.head)
	{
		monptr = ptr->data;

		send_multiline_item(client_p, "%s", monptr->name);
	}

	send_multiline_fini(client_p, NULL);
	sendto_one(client_p, form_str(RPL_ENDOFMONLIST),
			me.name, client_p->name);
}

static void
show_monitor_status(struct Client *client_p)
{
	struct Client *target_p;
	struct monitor *monptr;
	rb_dlink_node *ptr;

	send_multiline_init(client_p, ",", form_str(RPL_MONONLINE),
			me.name,
			client_p->name,
			"");

	RB_DLINK_FOREACH(ptr, client_p->localClient->monitor_list.head)
	{
		monptr = ptr->data;
		target_p = find_named_person(monptr->name);

		if (target_p != NULL)
		{
			send_multiline_item(client_p, "%s!%s@%s",
					target_p->name,
					target_p->username,
					target_p->host);
		}
	}

	send_multiline_fini(client_p, NULL);

	send_multiline_init(client_p, ",", form_str(RPL_MONOFFLINE),
			me.name,
			client_p->name,
			"");

	RB_DLINK_FOREACH(ptr, client_p->localClient->monitor_list.head)
	{
		monptr = ptr->data;

		if (find_named_person(monptr->name) == NULL)
		{
			send_multiline_item(client_p, "%s", monptr->name);
		}
	}

	send_multiline_fini(client_p, NULL);
}

static void
m_monitor(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	switch(parv[1][0])
	{
		case '+':
			if(parc < 3 || EmptyString(parv[2]))
			{
				sendto_one(client_p, form_str(ERR_NEEDMOREPARAMS),
						me.name, source_p->name, "MONITOR");
				return;
			}

			add_monitor(source_p, parv[2]);
			break;
		case '-':
			if(parc < 3 || EmptyString(parv[2]))
			{
				sendto_one(client_p, form_str(ERR_NEEDMOREPARAMS),
						me.name, source_p->name, "MONITOR");
				return;
			}

			del_monitor(source_p, parv[2]);
			break;

		case 'C':
		case 'c':
			clear_monitor(source_p);
			break;

		case 'L':
		case 'l':
			list_monitor(source_p);
			break;

		case 'S':
		case 's':
			show_monitor_status(source_p);
			break;

		default:
			break;
	}
}
