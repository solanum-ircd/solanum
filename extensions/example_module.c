/************************************************************************
 *   IRC - Internet Relay Chat, doc/example_module.c
 *   Copyright (C) 2001 Hybrid Development Team
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* List of ircd includes from ../include/ */
#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "batch.h"

/* This string describes the module. Always declare it a static const char[].
 * It is preferred for stylistic reasons to put it first.
 *
 * Make it short, sweet, and to the point.
 */
static const char example_desc[] = "This is an example Solanum module.";

/* Declare the void's initially up here, as modules dont have an
 * include file, we will normally have client_p, source_p, parc
 * and parv[] where:
 *
 * client_p == client issuing command
 * source_p == where the command came from
 * parc     == the number of parameters
 * parv     == an array of the parameters
 */

static void munreg_test(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static void mclient_test(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static void mserver_test(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static void mrclient_test(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static void moper_test(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

/* Show the commands this module can handle in a msgtab
 * and give the msgtab a name, here its test_msgtab
 */

struct Message test_msgtab = {
	"TEST",		/* the /COMMAND you want */
	0,		/* SET TO ZERO -- number of times command used by clients */
	0,		/* SET TO ZERO -- number of times command used by clients */
	0,		/* SET TO ZERO -- number of times command used by clients */
	0,		/* ALWAYS SET TO 0 */

	/* the functions to call for each handler.  If not using the generic
	 * handlers, the first param is the function to call, the second is the
	 * required number of parameters.  NOTE: If you specify a min para of 2,
	 * then parv[1] must *also* be non-empty.
	 */
	{
		{munreg_test, 0},   /* function call for unregistered clients, 0 parms required */
		{mclient_test, 0},  /* function call for local clients, 0 parms required */
		{mrclient_test, 0}, /* function call for remote clients, 0 parms required */
		{mserver_test, 0},  /* function call for servers, 0 parms required */
		mg_ignore,          /* function call for ENCAP, unused in this test */
		{moper_test, 0}     /* function call for operators, 0 parms required */
	}
};
/*
 * There are also some macros for the above function calls and parameter counts.
 * Here's a list:
 *
 * mg_ignore:       ignore the command when it comes from certain types
 * mg_not_oper:     tell the client it requires being an operator
 * mg_reg:          prevent the client using this if registered
 * mg_unreg:        prevent the client using this if unregistered
 *
 * These macros assume a parameter count of zero; you do not set it.
 * For further details, see include/msg.h
 */


/* The mapi_clist_av1 indicates which commands (struct Message)
 * should be loaded from the module. The list should be terminated
 * by a NULL.
 */
mapi_clist_av1 test_clist[] = { &test_msgtab, NULL };

/* The mapi_hlist_av1 indicates which hook functions we need to be able to
 * call.  We need to declare an integer, then add the name of the hook
 * function to call and a pointer to this integer.  The list should be
 * terminated with NULLs.
 */
int doing_example_hook;
mapi_hlist_av1 test_hlist[] = {
	{ "doing_example_hook", &doing_example_hook, },
	{ NULL, NULL }
};

/* The mapi_hfn_list_av1 declares the hook functions which other modules can
 * call.  The first parameter is the name of the hook, the second is a void
 * returning function, with arbitrary parameters casted to (hookfn).  This
 * list must be terminated with NULLs.
 */
static void show_example_hook(void *unused);

mapi_hfn_list_av1 test_hfnlist[] = {
	{ "doing_example_hook", show_example_hook },
	{ NULL, NULL }
};

/* The mapi_cap_list_av2 declares the capabilities this module adds.  This is
 * for protocol usage. Here we declare both server and client capabilities.
 * The first parameter is the cap type (server or client). The second is the
 * name of the capability we wish to register. The third is the data attached
 * to the cap (typically NULL). The last parameter is a pointer to an integer
 * for the CAP index (recommended).
 */
unsigned int CAP_TESTCAP_SERVER, CAP_TESTCAP_CLIENT;
mapi_cap_list_av2 test_cap_list[] = {
	{ MAPI_CAP_SERVER, "TESTCAP", NULL, &CAP_TESTCAP_SERVER },
	{ MAPI_CAP_CLIENT, "testcap", NULL, &CAP_TESTCAP_CLIENT },
	{ 0, NULL, NULL, NULL }
};

/* If the module adds client-initiated batches,
 * add them here in a struct BatchHandler (one for each batch).
 */
static void test_batch_cb(struct Client *client_p, struct Client *source_p, struct Batch *batch, void *userdata);
static bool test_batch_child(struct Client *client_p, struct Client *source_p, struct Batch *parent, struct MsgBuf *child, void *userdata, const char **error);
struct BatchHandler test_batch_handler = {
	&test_batch_cb,	/* The callback invoked to process this batch */
	NULL,			/* A pointer to pass as the userdata parameter to the callback function */
	0,					/* Flags for the batch handler (see include/batch.h) */
	/* The types of batches that can be nested under this one.
	 * This should either be NULL (if no nesting is allowed or flags has BATCH_FLAG_ALLOW_ALL),
	 * or a callback function that returns true if the nesting is allowed.
	 */
	&test_batch_child
};

/* Here we tell it what to do when the module is loaded */
static int
modinit(void)
{
	/* The init function should return -1 on failure,
	   which will cause the module to be unloaded,
	   otherwise 0 to indicate success. */
	if (!register_batch_handler("test", &test_batch_handler))
	{
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
			"A batch type named test was already registered.");
		return -1;
	}

	return 0;
}

/* here we tell it what to do when the module is unloaded */
static void
moddeinit(void)
{
	remove_batch_handler("test");
}

/* DECLARE_MODULE_AV2() actually declare the MAPI header. */
DECLARE_MODULE_AV2(
	/* The first argument is the name */
	example,
	/* The second argument is the function to call on load */
	modinit,
	/* And the function to call on unload */
	moddeinit,
	/* Then the MAPI command list */
	test_clist,
	/* Next the hook list, if we have one. */
	test_hlist,
	/* Then the hook function list, if we have one */
	test_hfnlist,
	/* Then the caps list, if we have one */
	test_cap_list,
	/* Then the version number of this module (NULL for bundled) */
	NULL,
	/* And finally, the description of this module */
	example_desc);

/* Any of the above arguments can be NULL to indicate they aren't used. */

/*
 * mr_test
 *      parv[1] = parameter
 */

/* Here we have the functions themselves that we declared above,
 * and the fairly normal C coding
 */
static void
munreg_test(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc < 2)
	{
		sendto_one_notice(source_p, ":You are unregistered and sent no parameters");
	}
	else
	{
		sendto_one_notice(source_p, ":You are unregistered and sent parameter: %s", parv[1]);
	}

	/* illustration of how to call a hook function */
	call_hook(doing_example_hook, NULL);
}

/*
 * mclient_test
 *      parv[1] = parameter
 */
static void
mclient_test(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc < 2)
	{
		sendto_one_notice(source_p, ":You are a normal user, and sent no parameters");
	}
	else
	{
		sendto_one_notice(source_p, ":You are a normal user, and send parameters: %s", parv[1]);
	}

	/* illustration of how to call a hook function */
	call_hook(doing_example_hook, NULL);
}

/*
 * mrclient_test
 *      parv[1] = parameter
 */
static void
mrclient_test(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc < 2)
	{
		sendto_one_notice(source_p, ":You are a remote client, and sent no parameters");
	}
	else
	{
		sendto_one_notice(source_p, ":You are a remote client, and sent parameters: %s", parv[1]);
	}
}

/*
 * mserver_test
 *      parv[1] = parameter
 */
static void
mserver_test(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc < 2)
	{
		sendto_one_notice(source_p, ":You are a server, and sent no parameters");
	}
	else
	{
		sendto_one_notice(source_p, ":You are a server, and sent parameters: %s", parv[1]);
	}
}

/*
 * moper_test
 *      parv[1] = parameter
 */
static void
moper_test(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc < 2)
	{
		sendto_one_notice(source_p, ":You are an operator, and sent no parameters");
	}
	else
	{
		sendto_one_notice(source_p, ":You are an operator, and sent parameters: %s", parv[1]);
	}
}

static void
show_example_hook(void *unused)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "Called example hook!");
}

static void
test_batch_cb(struct Client *client_p, struct Client *source_p, struct Batch *batch, void *userdata)
{
	/* batches with no inner messages are no-op and will not call this handler */
	sendto_one_notice(source_p, ":You have sent a %s batch with %d message%s",
		batch->parent == NULL ? "top-level" : "nested",
		batch->len, batch->len == 1 ? "" : "s");
}

static bool
test_batch_child(struct Client *client_p, struct Client *source_p, struct Batch *parent, struct MsgBuf *child, void *userdata, const char **error)
{
	/* Allow test batches to be nested into each other */
	return !strcmp(child->para[2], "test");
}

/* END OF EXAMPLE MODULE */
