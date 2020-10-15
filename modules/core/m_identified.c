#include <stdinc.h>
#include <modules.h>
#include <messages.h>
#include <send.h>

static const char identified_desc[] = "Provides the IDENTIFIED server-to-server command";

static void m_identified(struct MsgBuf *, struct Client *, struct Client *, int, const char *[]);

static struct Message identified_msgtab = {
	"IDENTIFIED", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {m_identified, 3}, mg_ignore}
};

static mapi_clist_av1 identified_clist[] = {
	&identified_msgtab,
	NULL
};

static void identified_nick_change(void *);
static void identified_burst_client(void *);

static mapi_hfn_list_av1 identified_hfnlist[] = {
	{ "local_nick_change", identified_nick_change, HOOK_MONITOR },
	{ "remote_nick_change", identified_nick_change, HOOK_MONITOR },
	{ "burst_client", identified_burst_client },
	{ NULL, NULL }
};

static void m_identified(struct MsgBuf *msgbuf, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Client *target_p = find_person(parv[1]);
	const char *nick = parv[2];

	if (target_p == NULL)
		return;

	if (irccmp(target_p->name, nick))
		return;

	if (parc > 3 && irccmp(parv[3], "OFF") == 0)
		ClearIdentified(target_p);
	else
		SetIdentified(target_p);
}

static void identified_nick_change(void *data_)
{
	hook_cdata *data = data_;
	const char *oldnick = data->arg1, *newnick = data->arg2;

	if (irccmp(oldnick, newnick) != 0)
		ClearIdentified(data->client);
}

static void identified_burst_client(void *data_)
{
	hook_data_client *data = data_;

	if (IsIdentified(data->target))
		sendto_one(data->client, ":%s ENCAP * IDENTIFIED %s :%s", me.id, use_id(data->target), data->target->name);
}

DECLARE_MODULE_AV2(identified, NULL, NULL, identified_clist, NULL, identified_hfnlist, NULL, NULL, identified_desc);
