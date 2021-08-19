/* modules/m_sasl.c
 *   Copyright (C) 2006 Michael Tharp <gxti@partiallystapled.com>
 *   Copyright (C) 2006 charybdis development team
 *   Copyright (C) 2016 ChatLounge IRC Network Development Team
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
#include "hash.h"
#include "send.h"
#include "msg.h"
#include "modules.h"
#include "numeric.h"
#include "reject.h"
#include "s_serv.h"
#include "s_stats.h"
#include "string.h"
#include "s_newconf.h"
#include "s_conf.h"

static const char sasl_desc[] = "Provides SASL authentication support";

static void m_authenticate(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void me_sasl(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void me_mechlist(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

static void abort_sasl(void *);
static void abort_sasl_exit(void *);

static unsigned int CLICAP_SASL = 0;
static char mechlist_buf[BUFSIZE];

struct Message authenticate_msgtab = {
	"AUTHENTICATE", 0, 0, 0, 0,
	{{m_authenticate, 2}, {m_authenticate, 2}, mg_ignore, mg_ignore, mg_ignore, {m_authenticate, 2}}
};
struct Message sasl_msgtab = {
	"SASL", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_sasl, 5}, mg_ignore}
};
struct Message mechlist_msgtab = {
	"MECHLIST", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_mechlist, 2}, mg_ignore}
};

mapi_clist_av1 sasl_clist[] = {
	&authenticate_msgtab, &sasl_msgtab, &mechlist_msgtab, NULL
};
mapi_hfn_list_av1 sasl_hfnlist[] = {
	{ "new_local_user",	abort_sasl },
	{ "client_exit",	abort_sasl_exit },
	{ NULL, NULL }
};

static const char *
sasl_data(struct Client *client_p)
{
	return *mechlist_buf != 0 ? mechlist_buf : NULL;
}

static struct ClientCapability capdata_sasl = {
	.data = sasl_data,
	.flags = CLICAP_FLAGS_STICKY | CLICAP_FLAGS_PRIORITY,
};

mapi_cap_list_av2 sasl_cap_list[] = {
	{ MAPI_CAP_CLIENT, "sasl", &capdata_sasl, &CLICAP_SASL },
	{ 0, NULL, NULL, NULL },
};

static int
_modinit(void)
{
	memset(mechlist_buf, 0, sizeof mechlist_buf);
	return 0;
}

DECLARE_MODULE_AV2(sasl, _modinit, NULL, sasl_clist, NULL, sasl_hfnlist, sasl_cap_list, NULL, sasl_desc);

static void
m_authenticate(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p,
	int parc, const char *parv[])
{
	struct Client *agent_p = NULL;
	struct Client *saslserv_p = NULL;

	/* They really should use CAP for their own sake. */
	if(!IsCapable(source_p, CLICAP_SASL))
		return;

	if(source_p->localClient->sasl_next_retry > rb_current_time())
	{
		sendto_one(source_p, form_str(RPL_LOAD2HI), me.name, EmptyString(source_p->name) ? "*" : source_p->name, msgbuf_p->cmd);
		return;
	}

	if (strlen(client_p->id) == 3 || (source_p->preClient && !EmptyString(source_p->preClient->id)))
	{
		exit_client(client_p, client_p, client_p, "Mixing client and server protocol");
		return;
	}

	if (*parv[1] == ':' || strchr(parv[1], ' '))
	{
		exit_client(client_p, client_p, client_p, "Malformed AUTHENTICATE");
		return;
	}

	saslserv_p = find_named_client(ConfigFileEntry.sasl_service);
	if(saslserv_p == NULL || !IsService(saslserv_p))
	{
		sendto_one(source_p, form_str(ERR_SASLABORTED), me.name, EmptyString(source_p->name) ? "*" : source_p->name);
		return;
	}

	if(source_p->localClient->sasl_complete)
	{
		*source_p->localClient->sasl_agent = '\0';
		source_p->localClient->sasl_complete = 0;
	}

	if(strlen(parv[1]) > 400)
	{
		sendto_one(source_p, form_str(ERR_SASLTOOLONG), me.name, EmptyString(source_p->name) ? "*" : source_p->name);
		return;
	}

	if(!*source_p->id)
	{
		/* Allocate a UID. */
		rb_strlcpy(source_p->id, generate_uid(), sizeof(source_p->id));
		add_to_id_hash(source_p->id, source_p);
	}

	if(*source_p->localClient->sasl_agent)
		agent_p = find_id(source_p->localClient->sasl_agent);

	if(agent_p == NULL)
	{
		if (!strcmp(parv[1], "*"))
		{
			sendto_one(source_p, form_str(ERR_SASLABORTED), me.name, EmptyString(source_p->name) ? "*" : source_p->name);
			source_p->localClient->sasl_out = 0;
			return;
		}

		sendto_one(saslserv_p, ":%s ENCAP %s SASL %s %s H %s %s %c",
					me.id, saslserv_p->servptr->name, source_p->id, saslserv_p->id,
					source_p->host, source_p->sockhost,
					IsSecure(source_p) ? 'S' : 'P');

		if (source_p->certfp != NULL)
			sendto_one(saslserv_p, ":%s ENCAP %s SASL %s %s S %s %s",
						me.id, saslserv_p->servptr->name, source_p->id, saslserv_p->id,
						parv[1], source_p->certfp);
		else
			sendto_one(saslserv_p, ":%s ENCAP %s SASL %s %s S %s",
						me.id, saslserv_p->servptr->name, source_p->id, saslserv_p->id,
						parv[1]);

		rb_strlcpy(source_p->localClient->sasl_agent, saslserv_p->id, IDLEN);
	}
	else
	{
		if (!strcmp(parv[1], "*"))
		{
			sendto_one(source_p, form_str(ERR_SASLABORTED), me.name, EmptyString(source_p->name) ? "*" : source_p->name);
			sendto_one(agent_p, ":%s ENCAP %s SASL %s %s D A", me.id, agent_p->servptr->name, source_p->id, agent_p->id);
			source_p->localClient->sasl_out = 0;
			return;
		}

		sendto_one(agent_p, ":%s ENCAP %s SASL %s %s C %s",
				me.id, agent_p->servptr->name, source_p->id, agent_p->id,
				parv[1]);
	}

	source_p->localClient->sasl_out++;
}

static void
me_sasl(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p,
	int parc, const char *parv[])
{
	struct Client *target_p, *agent_p;
	bool in_progress;

	/* Let propagate if not addressed to us, or if broadcast.
	 * Only SASL agents can answer global requests.
	 */
	if(strncmp(parv[2], me.id, 3))
		return;

	if((target_p = find_id(parv[2])) == NULL)
		return;

	if((agent_p = find_id(parv[1])) == NULL)
		return;

	if(source_p != agent_p->servptr) /* WTF?! */
		return;

	/* We only accept messages from SASL agents; these must have umode +S
	 * (so the server must be listed in a service{} block).
	 */
	if(!IsService(agent_p))
		return;

	/* If SASL has been aborted, we only want to track authentication failures. */
	in_progress = target_p->localClient->sasl_out != 0;

	/* Reject if someone has already answered. */
	if(*target_p->localClient->sasl_agent && strncmp(parv[1], target_p->localClient->sasl_agent, IDLEN))
		return;
	else if(!*target_p->localClient->sasl_agent && in_progress)
		rb_strlcpy(target_p->localClient->sasl_agent, parv[1], IDLEN);

	if(*parv[3] == 'C')
	{
		if (in_progress) {
			sendto_one(target_p, "AUTHENTICATE %s", parv[4]);
			target_p->localClient->sasl_messages++;
		}
	}
	else if(*parv[3] == 'D')
	{
		if(*parv[4] == 'F')
		{
			if (in_progress) {
				sendto_one(target_p, form_str(ERR_SASLFAIL), me.name, EmptyString(target_p->name) ? "*" : target_p->name);
			}
			/* Failures with zero messages are just "unknown mechanism" errors; don't count those. */
			if(target_p->localClient->sasl_messages > 0)
			{
				if(*target_p->name)
				{
					/* Allow 2 tries before rate-limiting as some clients try EXTERNAL
					 * then PLAIN right after it if the auth failed, causing the client to be
					 * rate-limited immediately and not being able to login with SASL.
					 */
					if (target_p->localClient->sasl_failures++ > 0)
						target_p->localClient->sasl_next_retry = rb_current_time() + (1 << MIN(target_p->localClient->sasl_failures + 1, 8));
				}
				else if(throttle_add((struct sockaddr*)&target_p->localClient->ip))
				{
					exit_client(target_p, target_p, &me, "Too many failed authentication attempts");
					return;
				}
			}
		}
		else if(*parv[4] == 'S')
		{
			if (in_progress) {
				sendto_one(target_p, form_str(RPL_SASLSUCCESS), me.name, EmptyString(target_p->name) ? "*" : target_p->name);
				target_p->localClient->sasl_failures = 0;
				target_p->localClient->sasl_complete = 1;
				ServerStats.is_ssuc++;
			}
		}
		*target_p->localClient->sasl_agent = '\0'; /* Blank the stored agent so someone else can answer */
		target_p->localClient->sasl_messages = 0;
	}
	else if(*parv[3] == 'M')
	{
		if (in_progress) {
			sendto_one(target_p, form_str(RPL_SASLMECHS), me.name, EmptyString(target_p->name) ? "*" : target_p->name, parv[4]);
		}
	}
}

static void
me_mechlist(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p,
	    int parc, const char *parv[])
{
	rb_strlcpy(mechlist_buf, parv[1], sizeof mechlist_buf);
}

/* If the client never finished authenticating but is
 * registering anyway, abort the exchange.
 */
static void
abort_sasl(void *data_)
{
	struct Client *data = data_;
	if(data->localClient->sasl_out == 0 || data->localClient->sasl_complete)
		return;

	data->localClient->sasl_out = data->localClient->sasl_complete = 0;
	ServerStats.is_sbad++;

	if(!IsClosing(data))
		sendto_one(data, form_str(ERR_SASLABORTED), me.name, EmptyString(data->name) ? "*" : data->name);

	if(*data->localClient->sasl_agent)
	{
		struct Client *agent_p = find_id(data->localClient->sasl_agent);
		if(agent_p)
		{
			sendto_one(agent_p, ":%s ENCAP %s SASL %s %s D A", me.id, agent_p->servptr->name,
					data->id, agent_p->id);
			return;
		}
	}

	sendto_server(NULL, NULL, CAP_TS6|CAP_ENCAP, NOCAPS, ":%s ENCAP * SASL %s * D A", me.id,
			data->id);
}

static void
abort_sasl_exit(void *data_)
{
	hook_data_client_exit *data = data_;
	if (data->target->localClient)
		abort_sasl(data->target);
}
