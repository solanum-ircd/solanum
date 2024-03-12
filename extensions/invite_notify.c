#include <stdinc.h>
#include <modules.h>
#include <msgbuf.h>
#include <client.h>
#include <hash.h>
#include <send.h>
#include <s_conf.h>
#include <s_serv.h>

static const char inv_notify_desc[] = "Notifies channel on /invite and provides the invite-notify client capability";

static void hook_invite(void *);
static void m_invited(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static unsigned int CAP_INVITE_NOTIFY;

mapi_hfn_list_av1 inv_notify_hfnlist[] = {
	{ "invite", hook_invite, HOOK_MONITOR },
	{ NULL, NULL }
};

struct Message invited_msgtab = {
	"INVITED", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {m_invited, 4}, mg_ignore}
};

mapi_cap_list_av2 inv_notify_caplist[] = {
	{ MAPI_CAP_CLIENT, "invite-notify", NULL, &CAP_INVITE_NOTIFY },
	{ 0, NULL, NULL, NULL }
};

mapi_clist_av1 inv_notify_clist[] = { &invited_msgtab, NULL };

static void
invite_notify(struct Client *source, struct Client *target, struct Channel *channel)
{
	sendto_channel_local_with_capability(source, CHFL_CHANOP, CAP_INVITE_NOTIFY, 0, channel,
		":%s!%s@%s INVITE %s %s", source->name, source->username,
		source->host, target->name, channel->chname);

	if (!ConfigChannel.invite_notify_notice)
		return;

	sendto_channel_local_with_capability(source, CHFL_CHANOP, 0, CAP_INVITE_NOTIFY, channel,
		":%s NOTICE %s :%s is inviting %s to %s.",
		me.name, channel->chname, source->name, target->name, channel->chname);
}

static void
hook_invite(void *data_)
{
	hook_data_channel_approval *data = data_;

	if (data->approved)
		/* Don't notify if another hook is rejecting the invite.
		 * This won't work if the other hook is after us... but really, it's
		 * the thought that counts.
		 */
		return;

	sendto_server(NULL, NULL, CAP_TS6 | CAP_ENCAP, 0, "ENCAP * INVITED %s %s %s",
	              use_id(data->client), use_id(data->target),
	              data->chptr->chname);
	invite_notify(data->client, data->target, data->chptr);
}

static void
m_invited(struct MsgBuf *msgbuf, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	struct Client *inviter = find_person(parv[1]);
	struct Client *target = find_person(parv[2]);
	struct Channel *chptr = find_channel(parv[3]);

	if (inviter == NULL || target == NULL || chptr == NULL)
		return;

	invite_notify(inviter, target, chptr);
}

DECLARE_MODULE_AV2("invite_notify", NULL, NULL, inv_notify_clist, NULL, inv_notify_hfnlist, inv_notify_caplist, NULL, inv_notify_desc);
