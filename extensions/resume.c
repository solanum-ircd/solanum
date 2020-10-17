#include "stdinc.h"
#include "channel.h"
#include "client.h"
#include "hash.h"
#include "hostmask.h"
#include "ircd.h"
#include "send.h"
#include "packet.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "msg.h"
#include "modules.h"

static const char resume_desc[] = "Provides RESUME and BRB";

#define BRB_TIMEOUT 300

static int _modinit(void);
static void _moddeinit(void);
static void m_resume(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void m_resumed(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void m_brb(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static bool resume_visible(struct Client *);
static void hook_cap_change(void *);
static void hook_umode_changed(void *);
static void hook_client_exit(void *);
static void hook_sendq_cleared(void *);

static unsigned int CLICAP_RESUME = 0;
static unsigned int CAP_RESUME = 0;

static rb_dictionary *resume_tree;
static rb_dlink_list brb_list;
static struct ev_entry *resume_check_ev;

static struct Message resume_msgtab = {
	"RESUME", 0, 0, 0, 0,
	{{m_resume, 2}, {m_resume, 2}, mg_ignore, mg_ignore, mg_ignore, {m_resume, 2}}
};

static struct Message resumed_msgtab = {
	"RESUMED", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, {m_resumed, 2}, mg_ignore, mg_ignore, mg_ignore}
};

static struct Message brb_msgtab = {
	"BRB", 0, 0, 0, 0,
	{mg_unreg, {m_brb, 1}, mg_ignore, mg_ignore, mg_ignore, mg_ignore}
};

static mapi_clist_av1 resume_clist[] = {
	&resume_msgtab,
	&resumed_msgtab,
	&brb_msgtab,
	NULL
};

static mapi_hfn_list_av1 resume_hfnlist[] = {
	{ "cap_change", hook_cap_change },
	{ "umode_changed", hook_umode_changed },
	{ "client_exit", hook_client_exit },
	{ "sendq_cleared", hook_sendq_cleared },
	{ NULL, NULL }
};

static struct ClientCapability resume_clicap = {
	.visible = resume_visible
};

mapi_cap_list_av2 resume_cap_list[] = {
	{ MAPI_CAP_CLIENT, "resume", &resume_clicap, &CLICAP_RESUME },
	{ MAPI_CAP_SERVER, "RESUME", NULL, &CAP_RESUME },
	{ 0, NULL, NULL, NULL }
};

DECLARE_MODULE_AV2(resume, _modinit, _moddeinit, resume_clist, NULL, resume_hfnlist, resume_cap_list, NULL, resume_desc);

enum brb_status
{
	BRB_NO,
	BRB_ARMED,
	BRB_DONE
};

struct resume_session
{
	struct Client *owner;
	rb_dlink_node brb_node;
	char *reason;
	time_t brb_time;
	enum brb_status brb;
	unsigned char id[14];
	unsigned char key[16];
};

static int cmp_resume(const void *a_, const void *b_)
{
	const char *a = a_, *b = b_;
	return memcmp(a, b, sizeof ((struct resume_session){0}).id);
}

static bool resume_visible(struct Client *client)
{
	assert(MyConnect(client));
	return IsSSL(client) && !IsOper(client);
}

static void resume_check(void *unused)
{
	rb_dlink_node *n, *tmp;
	time_t now = rb_current_time();
	static char reason[BUFSIZE];

	RB_DLINK_FOREACH_SAFE(n, tmp, brb_list.head)
	{
		struct resume_session *rs = n->data;
		if (now < rs->brb_time + BRB_TIMEOUT)
			break;
		snprintf(reason, sizeof reason, "BRB: %s", rs->owner->user->away);
		exit_client(NULL, rs->owner, rs->owner, reason);
	}
}

static void sync_resumed_user(struct Client *target, bool brb)
{
	rb_dlink_node *ptr;
	buf_head_t tempq;

	if (brb)
	{
		tempq = target->localClient->buf_sendq;
		rb_linebuf_newbuf(&target->localClient->buf_sendq);

		/* XXX assumes the welcome can be sent instantly */
		user_welcome(target);
		sendto_one(target, ":%s NOTE RESUME PLAYBACK :History playback starts here", me.name);
		send_queued(target);
		target->localClient->buf_sendq = tempq;
	}
	else
	{
		user_welcome(target);
	}

	RB_DLINK_FOREACH(ptr, target->user->channel.head)
	{
		struct membership *msptr = ptr->data;
		struct Channel *chptr = msptr->chptr;
		char mode[10], modeval[NICKLEN * 2 + 2], *mptr = mode;

		*modeval = '\0';

		sendto_one(target, ":%s!%s@%s JOIN %s", target->name, target->username, target->host, chptr->chname);
		channel_member_names(chptr, target, 1);

		if (is_chanop(msptr))
		{
			*mptr++ = 'o';
			strcat(modeval, target->name);
			strcat(modeval, " ");
		}

		if (is_voiced(msptr))
		{
			*mptr++ = 'v';
			strcat(modeval, target->name);
		}

		if (*mode != '\0')
			sendto_one(target, ":%s MODE %s +%s %s", me.name, chptr->chname, mode, modeval);
	}

	sendto_one(target, "RESUME SUCCESS %s", target->name);

	if (!brb)
		sendto_one(target, ":%s WARN RESUME HISTORY_LOST :No history was preserved for this session", me.name);
}

static void announce_resume(struct Client *client, const char *oldhost, const char *status)
{
	rb_dlink_node *n;

	sendto_server(client, NULL, CAP_RESUME | CAP_TS6, 0, ":%s RESUMED %s%s%s", use_id(client), client->host,
		status ? " " : "",
		status ? status : "");
	sendto_server(client, NULL, CAP_EUID | CAP_TS6, CAP_RESUME, ":%s CHGHOST %s %s", use_id(client), use_id(client), client->host);
	sendto_server(client, NULL, CAP_ENCAP | CAP_TS6, CAP_RESUME | CAP_EUID, ":%s ENCAP * CHGHOST %s %s", use_id(client), use_id(client), client->host);

	sendto_common_channels_local_butone(client, CLICAP_RESUME, 0,
		":%s!%s@%s RESUMED %s%s%s", client->name, client->username, oldhost, client->host,
		status ? " " : "",
		status ? status : "");
	sendto_common_channels_local_butone(client, 0, CLICAP_RESUME,
		":%s!%s@%s QUIT :Client reconnected", client->name, client->username, oldhost);

	RB_DLINK_FOREACH(n, client->user->channel.head)
	{
		struct membership *msptr = n->data;
		struct Channel *chptr = msptr->chptr;
		char mode[10], modeval[NICKLEN * 2 + 2], *mptr = mode;

		*modeval = '\0';

		if (is_chanop(msptr))
		{
			*mptr++ = 'o';
			strcat(modeval, client->name);
			strcat(modeval, " ");
		}

		if (is_voiced(msptr))
		{
			*mptr++ = 'v';
			strcat(modeval, client->name);
		}

		*mptr = '\0';

		sendto_channel_local_with_capability_butone(client, ALL_MEMBERS, NOCAPS, CLICAP_EXTENDED_JOIN | CLICAP_RESUME, chptr,
			":%s!%s@%s JOIN %s", client->name, client->username, client->host, chptr->chname);
		sendto_channel_local_with_capability_butone(client, ALL_MEMBERS, CLICAP_EXTENDED_JOIN, CLICAP_RESUME, chptr,
			":%s!%s@%s JOIN %s %s :%s", client->name, client->username, client->host, chptr->chname,
			EmptyString(client->user->suser) ? "*" : client->user->suser, client->info);

		if(*mode)
			sendto_channel_local_with_capability_butone(client, ALL_MEMBERS, NOCAPS, CLICAP_RESUME, chptr,
				":%s MODE %s +%s %s", client->servptr->name, chptr->chname, mode, modeval);
	}

	/* Resend away message to away-notify enabled clients. */
	if (client->user->away)
		sendto_common_channels_local_butone(client, CLICAP_AWAY_NOTIFY, CLICAP_RESUME,
			":%s!%s@%s AWAY :%s", client->name, client->username, client->host, client->user->away);
}

static void enable_resume(struct Client *client_p)
{
	struct resume_session *rs = rb_malloc(sizeof *rs);
	static unsigned char token[sizeof rs->id + sizeof rs->key];
	char *b64token;
	rb_get_random(rs->id, sizeof rs->id);
	rb_get_random(rs->key, sizeof rs->key);
	rs->owner = client_p;
	rs->brb = BRB_NO;
	rs->reason = NULL;
	client_p->localClient->resume = rs;
	rb_dictionary_add(resume_tree, rs->id, rs);
	memcpy(token, rs->id, sizeof rs->id);
	memcpy(token + sizeof rs->id, rs->key, sizeof rs->key);
	b64token = (char *)rb_base64_encode(token, sizeof token);
	sendto_one(client_p, "RESUME TOKEN %s.%s", b64token, me.name);
	rb_free(b64token);
}

static void disable_resume(struct Client *client_p)
{
	struct resume_session *rs = client_p->localClient->resume;
	if (rs == NULL)
		return;
	client_p->localClient->resume = NULL;
	client_p->localClient->caps &= ~CLICAP_RESUME;
	if (rs->brb == BRB_DONE)
		rb_dlinkDelete(&rs->brb_node, &brb_list);
	if (rs->reason != NULL)
		rb_free(rs->reason);
	rb_dictionary_delete(resume_tree, rs->id);
	rb_free(rs);
}

static int memneq(const void *a_, const void *b_, size_t s)
{
	volatile const unsigned char *a = a_, *b = b_;
	volatile int r = 0;
	for (size_t i = 0; i < s; i++)
	{
		r |= a[i] ^ b[i];
	}
	return r;
}

static bool invalidate_token(const char *tokstr)
{
	int tl;
	struct rb_dictionary_element *elem;
	struct resume_session *rs;
	char *dot = strchr(tokstr, '.');
	unsigned char *token = rb_base64_decode((const unsigned char *)tokstr,
		dot != NULL ? dot - tokstr : strlen(tokstr), &tl);
	struct Client *owner;

	if (tl != sizeof rs->id + sizeof rs->key)
	{
		rb_free(token);
		return false;
	}

	elem = rb_dictionary_find(resume_tree, token);
	if (!elem)
	{
		rb_free(token);
		return false;
	}

	rs = elem->data;
	if (memneq(token + sizeof rs->id, rs->key, sizeof rs->key))
	{
		rb_free(token);
		return false;
	}

	rb_free(token);

	owner = rs->owner;
	disable_resume(owner);
	enable_resume(owner);
	return true;
}

static void resync_connids(struct Client *client)
{
	rb_dlink_node *n;
	RB_DLINK_FOREACH(n, client->localClient->connids.head)
	{
		uint32_t connid = RB_POINTER_TO_UINT(n->data);
		del_from_cli_connid_hash(connid);
		add_to_cli_connid_hash(client, connid);
	}
}

static bool recheck_kline(struct Client *client)
{
	struct ConfItem *aconf = find_kline(client);

	if(aconf == NULL)
		return false;

	if(IsExemptKline(client))
	{
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
			"KLINE over-ruled for %s, client is kline_exempt [%s@%s]",
			get_client_name(client, HIDE_IP),
			aconf->user, aconf->host);
		return false;
	}

	sendto_realops_snomask(SNO_GENERAL, L_ALL,
		"KLINE active for %s",
		get_client_name(client, HIDE_IP));

	notify_banned_client(client, aconf, K_LINED);
	return true;
}

static void m_resume(struct MsgBuf *msgbuf, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	/* find resume session */
	struct rb_dictionary_element *elem;
	struct resume_session *rs;
	int tl;

	char *dot = strchr(parv[1], '.');

	assert(client_p == source_p);

	if (!IsSSL(source_p))
	{
		sendto_one(source_p, ":%s FAIL RESUME INSECURE_SESSION :You must use TLS to resume sessions", me.name);
		if (invalidate_token(parv[1]))
			sendto_one_notice(source_p, "*** Your resume token was recognized, but has now been destroyed to protect against eavesdropping attacks.");
		return;
	}

	if (!IsUnknown(source_p))
	{
		sendto_one(source_p, ":%s FAIL RESUME REGISTRATION_IS_COMPLETED :You have already registered", me.name);
		return;
	}

	if (source_p->localClient->sasl_agent[0] != '\0' || source_p->localClient->sasl_complete)
	{
		sendto_one(source_p, ":%s FAIL RESUME CANNOT_RESUME :You must resume before starting SASL", me.name);
		return;
	}

	if (dot != NULL && dot[1] != '\0' && irccmp(dot + 1, me.name))
	{
		sendto_one(source_p, ":%s FAIL RESUME WRONG_SERVER %s :This token must be redeemed on %s", me.name, dot + 1, dot + 1);
		return;
	}

	unsigned char *token = rb_base64_decode((const unsigned char *)parv[1],
		dot != NULL ? dot - parv[1] : strlen(parv[1]), &tl);

	if (tl != sizeof rs->id + sizeof rs->key)
	{
		sendto_one(source_p, ":%s FAIL RESUME INVALID_TOKEN :Resume token unrecognized", me.name);
		rb_free(token);
		return;
	}

	elem = rb_dictionary_find(resume_tree, token);

	if (!elem)
	{
		sendto_one(source_p, ":%s FAIL RESUME INVALID_TOKEN :Resume token unrecognized", me.name);
		rb_free(token);
		return;
	}

	rs = elem->data;

	if (memneq(token + sizeof rs->id, rs->key, sizeof rs->key))
	{
		sendto_one(source_p, ":%s FAIL RESUME INVALID_TOKEN :Resume token unrecognized", me.name);
		rb_free(token);
		return;
	}

	rb_free(token);

	struct Client *victim = rs->owner;
	enum brb_status brb = rs->brb;
	char *reason = rs->reason;
	rs->reason = NULL;

	disable_resume(victim);

	if (IsOper(victim))
	{
		sendto_one(source_p, ":%s FAIL RESUME CANNOT_RESUME :Cowardly refusing to resume an oper", me.name);
		rb_free(reason);
		return;
	}

	rb_fde_t *tempF;
	struct _ssl_ctl *tempssl;
	struct ws_ctl *tempws;
	buf_head_t tempq;
	unsigned int tempcaps;
	rb_dlink_list templ;
	static char temphost[(HOSTLEN > HOSTIPLEN ? HOSTLEN : HOSTIPLEN) + 1];

	tempF = source_p->localClient->F;
	source_p->localClient->F = victim->localClient->F;
	victim->localClient->F = tempF;
	rb_setselect(victim->localClient->F, RB_SELECT_READ, read_packet, victim);

	tempssl = source_p->localClient->ssl_ctl;
	source_p->localClient->ssl_ctl = victim->localClient->ssl_ctl;
	victim->localClient->ssl_ctl = tempssl;

	tempws = source_p->localClient->ws_ctl;
	source_p->localClient->ws_ctl = victim->localClient->ws_ctl;
	victim->localClient->ws_ctl = tempws;

	templ = source_p->localClient->connids;
	source_p->localClient->connids = victim->localClient->connids;
	victim->localClient->connids = templ;
	resync_connids(source_p);
	resync_connids(victim);

	tempcaps = source_p->localClient->caps;
	source_p->localClient->caps = victim->localClient->caps;
	victim->localClient->caps = tempcaps;

	strcpy(source_p->orighost, victim->orighost);
	if (!IsIPSpoof(victim))
	{
		strcpy(victim->orighost, source_p->host);
	}

	strcpy(temphost, source_p->sockhost);
	strcpy(source_p->sockhost, victim->sockhost);
	strcpy(victim->sockhost, temphost);

	strcpy(temphost, victim->host);
	if (!IsIPSpoof(victim) && !IsDynSpoof(victim))
	{
		strcpy(victim->host, source_p->host);
		strcpy(source_p->host, temphost);
	}

	if (irccmp(victim->host, victim->orighost))
		SetDynSpoof(victim);
	else
		ClearDynSpoof(victim);

	del_from_hostname_hash(source_p->orighost, victim);
	add_to_hostname_hash(victim->orighost, victim);

	rb_linebuf_donebuf(&victim->localClient->buf_recvq);
	tempq = source_p->localClient->buf_recvq;
	source_p->localClient->buf_recvq = victim->localClient->buf_recvq;
	victim->localClient->buf_recvq = tempq;

	if (source_p->localClient->resume != NULL)
	{
		victim->localClient->resume = source_p->localClient->resume;
		victim->localClient->resume->owner = victim;
		source_p->localClient->resume = NULL;
	}

	victim->localClient->sasl_complete = 0;
	*victim->localClient->sasl_agent = '\0';

	exit_client(source_p, source_p, source_p, "Connection resumed");

	if (recheck_kline(victim))
	{
		rb_free(reason);
		return;
	}

	if (brb != BRB_DONE)
		rb_linebuf_donebuf(&victim->localClient->buf_sendq);

	ClearFlush(victim);
	sync_resumed_user(victim, brb == BRB_DONE);

	if (IsAnyDead(victim))
	{
		rb_free(reason);
		return;
	}

	if (brb == BRB_DONE)
	{
		if (reason == NULL)
		{
			free_away(victim);
			sendto_server(victim, NULL, CAP_TS6, NOCAPS, ":%s AWAY", use_id(victim));
			sendto_common_channels_local_butone(victim, CLICAP_AWAY_NOTIFY, NOCAPS, ":%s!%s@%s AWAY",
							    victim->name, victim->username, temphost);
		}
		else if (strncmp(victim->user->away, reason, AWAYLEN - 1))
		{
			rb_strlcpy(victim->user->away, reason, AWAYLEN);
			sendto_server(victim, NULL, CAP_TS6, NOCAPS, ":%s AWAY :%s", use_id(victim), victim->user->away);
			sendto_common_channels_local_butone(victim, CLICAP_AWAY_NOTIFY, NOCAPS,
				":%s!%s@%s AWAY :%s", victim->name, victim->username, temphost, victim->user->away);
		}
	}
	rb_free(reason);

	const char *status = brb == BRB_DONE ? "ok" : NULL;
	announce_resume(victim, temphost, status);

	if (!IsIPSpoof(victim))
		sendto_server(NULL, NULL, CAP_EUID | CAP_TS6, 0, ":%s ENCAP * REALHOST %s", use_id(victim), victim->orighost);
}

static void m_resumed(struct MsgBuf *msgbuf, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	const char *status = NULL;
	char oldhost[HOSTLEN + 1];
	if (parc >= 3)
		status = parv[2];
	rb_strlcpy(oldhost, source_p->host, sizeof oldhost);
	rb_strlcpy(source_p->host, parv[1], sizeof source_p->host);
	announce_resume(source_p, oldhost, status);
}

static void do_brb(struct Client *client, const char *reason)
{
	struct resume_session *rs = client->localClient->resume;
	rs->brb = BRB_DONE;
	sendto_one(client, "BRB %d", BRB_TIMEOUT);
	send_queued(client);
	SetFlush(client);
	rb_close(client->localClient->F);
	client->localClient->F = NULL;
	client->localClient->lasttime = rb_current_time() + BRB_TIMEOUT;
	rs->brb_time = rb_current_time();
	rb_dlinkAddTail(rs, &rs->brb_node, &brb_list);
	if (rs->reason != NULL)
	{
		rb_free(rs->reason);
		rs->reason = NULL;
	}

	if (client->user->away == NULL)
		allocate_away(client);
	if (strncmp(client->user->away, reason, AWAYLEN - 1))
	{
		if (client->user->away[0] != '\0')
			rs->reason = rb_strdup(client->user->away);
		rb_strlcpy(client->user->away, reason, AWAYLEN);
		sendto_server(client, NULL, CAP_TS6, NOCAPS, ":%s AWAY :%s", use_id(client), client->user->away);
		sendto_common_channels_local_butone(client, CLICAP_AWAY_NOTIFY, NOCAPS,
			":%s!%s@%s AWAY :%s", client->name, client->username, client->host, client->user->away);
	}
}

static void m_brb(struct MsgBuf *msgbuf, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	struct resume_session *rs = source_p->localClient->resume;

	if (rs == NULL)
	{
		sendto_one(source_p, ":%s FAIL BRB CANNOT_BRB :You do not have a resume token. CAP REQ resume first.", me.name);
		return;
	}

	assert(rs->brb == BRB_NO);

	rb_linebuf_donebuf(&source_p->localClient->buf_recvq);
	rb_setselect(source_p->localClient->F, RB_SELECT_READ, NULL, NULL);

	if (rb_linebuf_len(&source_p->localClient->buf_sendq) != 0)
	{
		rs->brb = BRB_ARMED;
		rs->reason = rb_strdup(parv[1]);
	}
	else
	{
		do_brb(source_p, parv[1]);
	}
}

static int _modinit(void)
{
	rb_dlink_node *n;
	resume_tree = rb_dictionary_create("resume", cmp_resume);
	RB_DLINK_FOREACH(n, lclient_list.head)
	{
		struct Client *client = n->data;
		struct resume_session *rs = client->localClient->resume;
		bool is_resume = (client->localClient->caps & CLICAP_RESUME) != 0;

		assert(!is_resume || rs);

		if (!is_resume && rs != NULL)
		{
			client->localClient->resume = NULL;
			rb_free(rs);
		}
		else if (rs != NULL)
		{
			if (IsOper(client))
				disable_resume(client);
			else
				rb_dictionary_add(resume_tree, rs->id, rs);
		}
	}

	resume_check_ev = rb_event_add("resume_check", resume_check, NULL, 10);

	return 0;
}

static void _moddeinit(void)
{
	rb_dictionary_destroy(resume_tree, NULL, NULL);
	rb_event_delete(resume_check_ev);
}

static void hook_cap_change(void *data_)
{
	hook_data_cap_change *data = data_;

	if (data->del & CLICAP_RESUME)
		disable_resume(data->client);
	else if (data->add & CLICAP_RESUME)
		enable_resume(data->client);
}

static void hook_umode_changed(void *data_)
{
	hook_data_umode_changed *data = data_;
	bool was_oper = !!(data->oldumodes & UMODE_OPER);

	if (!MyClient(data->client))
		return;

	if (was_oper && !IsOper(data->client) && IsCapable(data->client, CLICAP_CAP_NOTIFY))
		sendto_one(data->client, "CAP %s NEW :resume", data->client->name);
	if (!was_oper && IsOper(data->client) && IsCapable(data->client, CLICAP_CAP_NOTIFY))
		sendto_one(data->client, "CAP %s DEL :resume", data->client->name);

	if (MyOper(data->client) && data->client->localClient->resume != NULL)
	{
		disable_resume(data->client);
		sendto_one_notice(data->client, "You're too cool for resume");
	}
}

static void hook_client_exit(void *data_)
{
	hook_data_client_exit *data = data_;

	if (!MyClient(data->target))
		return;

	if (data->target->localClient->resume)
		disable_resume(data->target);
}

static void hook_sendq_cleared(void *data_)
{
	hook_data_client *data = data_;
	struct resume_session *rs = data->client->localClient->resume;

	if (rs != NULL && rs->brb == BRB_ARMED)
	{
		do_brb(data->client, rs->reason);
	}
}
