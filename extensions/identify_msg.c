#include <stdinc.h>
#include <modules.h>
#include <msgbuf.h>

static const char identify_msg_desc[] = "Provides the solanum.chat/identify-msg client capability";

static void identmsg_outbound(void *);
unsigned int CLICAP_IDENTIFY_MSG = 0;

mapi_cap_list_av2 identmsg_cap_list[] = {
	{ MAPI_CAP_CLIENT, "solanum.chat/identify-msg", NULL, &CLICAP_IDENTIFY_MSG },
	{ 0, NULL, NULL, NULL }
};

static mapi_hfn_list_av1 identmsg_hfnlist[] = {
	{ "outbound_msgbuf", identmsg_outbound },
	{ NULL, NULL }
};

static void identmsg_outbound(void *data_)
{
	hook_data *data = data_;
	struct MsgBuf *msgbuf = data->arg1;

	if (IsIdentified(data->client))
		msgbuf_append_tag(msgbuf, "solanum.chat/identified", NULL, CLICAP_IDENTIFY_MSG);
}

DECLARE_MODULE_AV2(identify_msg, NULL, NULL, NULL, NULL, identmsg_hfnlist, identmsg_cap_list, NULL, identify_msg_desc);
