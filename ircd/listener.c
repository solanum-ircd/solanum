/*
 *  ircd-ratbox: A slightly useful ircd.
 *  listener.c: Listens on a port.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *  Copyright (C) 2021 Ariadne Conill
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
#include "setup.h"
#include "listener.h"
#include "client.h"
#include "match.h"
#include "ircd.h"
#include "ircd_defs.h"
#include "numeric.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "s_stats.h"
#include "send.h"
#include "authproc.h"
#include "reject.h"
#include "hostmask.h"
#include "sslproc.h"
#include "wsproc.h"
#include "hash.h"
#include "s_assert.h"
#include "logger.h"

static rb_dlink_list listener_list = {};
static int accept_precallback(rb_fde_t *F, struct sockaddr *addr, rb_socklen_t addrlen, void *data);
static void accept_callback(rb_fde_t *F, int status, struct sockaddr *addr, rb_socklen_t addrlen, void *data);
static SSL_OPEN_CB accept_sslcallback;

static struct Listener *
make_listener(struct rb_sockaddr_storage *addr)
{
	struct Listener *listener = (struct Listener *) rb_malloc(sizeof(struct Listener));
	s_assert(0 != listener);
	listener->name = me.name;
	listener->F = NULL;

	memcpy(&listener->addr, addr, sizeof(listener->addr));
	return listener;
}

void
free_listener(struct Listener *listener)
{
	s_assert(NULL != listener);
	if(listener == NULL)
		return;

	rb_dlinkDelete(&listener->lnode, &listener_list);
	rb_free(listener);
}

#define PORTNAMELEN 6		/* ":31337" */

/*
 * get_listener_port - return displayable listener port
 */
static uint16_t
get_listener_port(const struct Listener *listener)
{
	return ntohs(GET_SS_PORT(&listener->addr[0]));
}

/*
 * get_listener_name - return displayable listener name and port
 */
const char *
get_listener_name(const struct Listener *listener)
{
	static char buf[BUFSIZE];

	snprintf(buf, sizeof(buf), "%s[%s/%u]",
			me.name, listener->name, get_listener_port(listener));
	return buf;
}

/*
 * show_ports - send port listing to a client
 * inputs       - pointer to client to show ports to
 * output       - none
 * side effects - show ports
 */
void
show_ports(struct Client *source_p)
{
	rb_dlink_node *n;

	RB_DLINK_FOREACH(n, listener_list.head)
	{
		struct Listener *listener = n->data;

		sendto_one_numeric(source_p, RPL_STATSPLINE,
			   form_str(RPL_STATSPLINE), 'P',
			   get_listener_port(listener),
			   IsOperAdmin(source_p) ? listener->name : me.name,
			   listener->ref_count, (listener->active) ? "active" : "disabled",
			   listener->sctp ? " sctp" : " tcp",
			   listener->ssl ? " ssl" : "");
	}
}

/*
 * inetport - create a listener socket in the AF_INET or AF_INET6 domain,
 * bind it to the port given in 'port' and listen to it
 * returns true (1) if successful false (0) on error.
 */

static int
inetport(struct Listener *listener)
{
	rb_fde_t *F;
	const char *errstr;
	int ret;

	if (listener->sctp) {
#ifdef HAVE_LIBSCTP
		/* only AF_INET6 sockets can have both AF_INET and AF_INET6 addresses */
		F = rb_socket(AF_INET6, SOCK_STREAM, IPPROTO_SCTP, "Listener socket");
#else
		F = NULL;
#endif
	} else {
		F = rb_socket(GET_SS_FAMILY(&listener->addr[0]), SOCK_STREAM, IPPROTO_TCP, "Listener socket");
	}

	memset(listener->vhost, 0, sizeof(listener->vhost));

	if (GET_SS_FAMILY(&listener->addr[0]) == AF_INET6) {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)&listener->addr[0];
		rb_inet_ntop(AF_INET6, &in6->sin6_addr, listener->vhost, sizeof(listener->vhost));
	} else if (GET_SS_FAMILY(&listener->addr[0]) == AF_INET) {
		struct sockaddr_in *in = (struct sockaddr_in *)&listener->addr[0];
		rb_inet_ntop(AF_INET, &in->sin_addr, listener->vhost, sizeof(listener->vhost));
	}

	if (GET_SS_FAMILY(&listener->addr[1]) == AF_INET6) {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)&listener->addr[1];
		strncat(listener->vhost, "&", sizeof(listener->vhost));
		rb_inet_ntop(AF_INET6, &in6->sin6_addr, &listener->vhost[strlen(listener->vhost)], sizeof(listener->vhost) - strlen(listener->vhost));
	} else if (GET_SS_FAMILY(&listener->addr[1]) == AF_INET) {
		struct sockaddr_in *in = (struct sockaddr_in *)&listener->addr[1];
		strncat(listener->vhost, "&", sizeof(listener->vhost));
		rb_inet_ntop(AF_INET, &in->sin_addr, &listener->vhost[strlen(listener->vhost)], sizeof(listener->vhost) - strlen(listener->vhost));
	}

	if (listener->vhost[0] != '\0') {
		listener->name = listener->vhost;
	}

	if (F == NULL) {
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				"Cannot open socket for listener on %s port %d",
				listener->sctp ? "SCTP" : "TCP",
				get_listener_port(listener));
		ilog(L_MAIN, "Cannot open socket for %s listener %s",
				listener->sctp ? "SCTP" : "TCP",
				get_listener_name(listener));
		return 0;
	}

	if (listener->sctp) {
		ret = rb_sctp_bindx(F, listener->addr, ARRAY_SIZE(listener->addr));
	} else {
		ret = rb_bind(F, (struct sockaddr *)&listener->addr[0]);
	}

	if (ret) {
		errstr = strerror(rb_get_sockerr(F));
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				"Cannot bind for listener on %s port %d: %s",
				listener->sctp ? "SCTP" : "TCP",
				get_listener_port(listener), errstr);
		ilog(L_MAIN, "Cannot bind for %s listener %s: %s",
				listener->sctp ? "SCTP" : "TCP",
				get_listener_name(listener), errstr);
		rb_close(F);
		return 0;
	}

	if(rb_listen(F, SOMAXCONN, listener->defer_accept))
	{
		errstr = strerror(rb_get_sockerr(F));
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
				"Cannot listen() for listener on %s port %d: %s",
				listener->sctp ? "SCTP" : "TCP",
				get_listener_port(listener), errstr);
		ilog(L_MAIN, "Cannot listen() for %s listener %s: %s",
				listener->sctp ? "SCTP" : "TCP",
				get_listener_name(listener), errstr);
		rb_close(F);
		return 0;
	}

	listener->F = F;

	rb_accept_tcp(listener->F, accept_precallback, accept_callback, listener);
	return 1;
}

static struct Listener *
find_listener(struct rb_sockaddr_storage *addr, int sctp)
{
	rb_dlink_node *n;
	struct Listener *last_closed = NULL;

	RB_DLINK_FOREACH(n, listener_list.head) {
		struct Listener *listener = n->data;

		if (listener->sctp != sctp)
			continue;

		for (int i = 0; i < ARRAY_SIZE(listener->addr); i++) {
			if (GET_SS_FAMILY(&addr[i]) != GET_SS_FAMILY(&listener->addr[i]))
				goto next;

			switch (GET_SS_FAMILY(&addr[i])) {
				case AF_INET:
				{
					struct sockaddr_in *in4 = (struct sockaddr_in *)&addr[i];
					struct sockaddr_in *lin4 = (struct sockaddr_in *)&listener->addr[i];
					if(in4->sin_addr.s_addr != lin4->sin_addr.s_addr ||
						in4->sin_port != lin4->sin_port)
					{
						goto next;
					}
					break;
				}

				case AF_INET6:
				{
					struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)&addr[i];
					struct sockaddr_in6 *lin6 =(struct sockaddr_in6 *)&listener->addr[i];
					if (!IN6_ARE_ADDR_EQUAL(&in6->sin6_addr, &lin6->sin6_addr) ||
						in6->sin6_port != lin6->sin6_port)
					{
						goto next;
					}
					break;
				}

				default:
					break;
			}
		}

		if (listener->F == NULL) {
			last_closed = listener;
		} else {
			return listener;
		}

next:
		continue;
	}
	return last_closed;
}


/*
 * add_tcp_listener- create a new listener
 * port - the port number to listen on
 * vhost_ip - if non-null must contain a valid IP address string in
 * the format "255.255.255.255"
 */
void
add_tcp_listener(int port, const char *vhost_ip, int family, int ssl, int defer_accept, int wsock)
{
	struct Listener *listener;
	struct rb_sockaddr_storage vaddr[ARRAY_SIZE(listener->addr)];

	/*
	 * if no port in conf line, don't bother
	 */
	if (port == 0)
		return;
	memset(&vaddr, 0, sizeof(vaddr));
	SET_SS_FAMILY(&vaddr[0], AF_UNSPEC);
	SET_SS_LEN(&vaddr[0], sizeof(struct sockaddr_storage));
	SET_SS_FAMILY(&vaddr[1], AF_UNSPEC);
	SET_SS_LEN(&vaddr[1], sizeof(struct sockaddr_storage));

	if (vhost_ip != NULL) {
		if (family == AF_INET) {
			if (rb_inet_pton(family, vhost_ip, &((struct sockaddr_in *)&vaddr[0])->sin_addr) <= 0)
				return;
		} else {
			if (rb_inet_pton(family, vhost_ip, &((struct sockaddr_in6 *)&vaddr[0])->sin6_addr) <= 0)
				return;
		}
	} else {
		switch(family) {
			case AF_INET:
				((struct sockaddr_in *)&vaddr[0])->sin_addr.s_addr = INADDR_ANY;
				break;
			case AF_INET6:
				memcpy(&((struct sockaddr_in6 *)&vaddr[0])->sin6_addr, &in6addr_any, sizeof(struct in6_addr));
				break;
			default:
				return;
		}
	}
	switch(family) {
		case AF_INET:
			SET_SS_LEN(&vaddr[0], sizeof(struct sockaddr_in));
			SET_SS_FAMILY(&vaddr[0], AF_INET);
			SET_SS_PORT(&vaddr[0], htons(port));
			break;
		case AF_INET6:
			SET_SS_LEN(&vaddr[0], sizeof(struct sockaddr_in6));
			SET_SS_FAMILY(&vaddr[0], AF_INET6);
			SET_SS_PORT(&vaddr[0], htons(port));
			break;
		default:
			break;
	}
	if ((listener = find_listener(vaddr, 0))) {
		if (listener->F != NULL)
			return;
	} else {
		listener = make_listener(vaddr);
		rb_dlinkAdd(listener, &listener->lnode, &listener_list);
	}

	listener->F = NULL;
	listener->ssl = ssl;
	listener->defer_accept = defer_accept;
	listener->sctp = 0;
	listener->wsock = wsock;

	if (inetport(listener)) {
		listener->active = 1;
	} else {
		close_listener(listener);
	}
}

/*
 * add_sctp_listener- create a new listener
 * port - the port number to listen on
 * vhost_ip1/2 - if non-null must contain a valid IP address string
 */
void
add_sctp_listener(int port, const char *vhost_ip1, const char *vhost_ip2, int ssl, int wsock)
{
	struct Listener *listener;
	struct rb_sockaddr_storage vaddr[ARRAY_SIZE(listener->addr)];

	/*
	 * if no port in conf line, don't bother
	 */
	if (port == 0)
		return;
	memset(&vaddr, 0, sizeof(vaddr));

	if (vhost_ip1 != NULL) {
		if (rb_inet_pton_sock(vhost_ip1, &vaddr[0]) <= 0)
			return;

		if (vhost_ip2 != NULL) {
			if (rb_inet_pton_sock(vhost_ip2, &vaddr[1]) <= 0)
				return;
		} else {
			SET_SS_FAMILY(&vaddr[1], AF_UNSPEC);
			SET_SS_LEN(&vaddr[1], sizeof(struct sockaddr_storage));
		}

		if (GET_SS_FAMILY(&vaddr[0]) == AF_INET && GET_SS_FAMILY(&vaddr[1]) == AF_INET6) {
			/* always put INET6 first */
			struct rb_sockaddr_storage tmp;
			tmp = vaddr[0];
			vaddr[0] = vaddr[1];
			vaddr[1] = tmp;
		}
	} else {
		memcpy(&((struct sockaddr_in6 *)&vaddr[0])->sin6_addr, &in6addr_any, sizeof(struct in6_addr));
		SET_SS_FAMILY(&vaddr[0], AF_INET6);
		SET_SS_LEN(&vaddr[0], sizeof(struct sockaddr_in6));

		SET_SS_FAMILY(&vaddr[1], AF_UNSPEC);
		SET_SS_LEN(&vaddr[1], sizeof(struct sockaddr_storage));
	}

	SET_SS_PORT(&vaddr[0], htons(port));
	SET_SS_PORT(&vaddr[1], htons(port));

	if ((listener = find_listener(vaddr, 1))) {
		if(listener->F != NULL)
			return;
	} else {
		listener = make_listener(vaddr);
		rb_dlinkAdd(listener, &listener->lnode, &listener_list);
	}

	listener->F = NULL;
	listener->ssl = ssl;
	listener->defer_accept = 0;
	listener->sctp = 1;
	listener->wsock = wsock;

	if (inetport(listener)) {
		listener->active = 1;
	} else {
		close_listener(listener);
	}
}

/*
 * close_listener - close a single listener
 */
void
close_listener(struct Listener *listener)
{
	s_assert(listener != NULL);
	if(listener == NULL)
		return;
	if(listener->F != NULL)
	{
		rb_close(listener->F);
		listener->F = NULL;
	}

	listener->active = 0;

	if(listener->ref_count)
		return;

	free_listener(listener);
}

/*
 * close_listeners - close and free all listeners that are not being used
 */
void
close_listeners(void)
{
	rb_dlink_node *n, *tn;

	/*
	 * close all 'extra' listening ports we have
	 */
	RB_DLINK_FOREACH_SAFE(n, tn, listener_list.head)
	{
		struct Listener *listener = n->data;

		close_listener(listener);
	}

	rb_close_pending_fds();
}

/*
 * add_connection - creates a client which has just connected to us on
 * the given fd. The sockhost field is initialized with the ip# of the host.
 * The client is sent to the auth module for verification, and not put in
 * any client list yet.
 */
static void
add_connection(struct Listener *listener, rb_fde_t *F, struct sockaddr *sai, struct sockaddr *lai)
{
	struct Client *new_client;
	bool defer = false;
	s_assert(NULL != listener);

	/*
	 * get the client socket name from the socket
	 * the client has already been checked out in accept_connection
	 */
	new_client = make_client(NULL);
	new_client->localClient->F = F;

	memcpy(&new_client->localClient->ip, sai, sizeof(struct rb_sockaddr_storage));
	memcpy(&new_client->preClient->lip, lai, sizeof(struct rb_sockaddr_storage));

	/*
	 * copy address to 'sockhost' as a string, copy it to host too
	 * so we have something valid to put into error messages...
	 */
	rb_inet_ntop_sock((struct sockaddr *)&new_client->localClient->ip, new_client->sockhost,
		sizeof(new_client->sockhost));

	rb_strlcpy(new_client->host, new_client->sockhost, sizeof(new_client->host));

	if (listener->sctp) {
		SetSCTP(new_client);
	}

	if (listener->ssl)
	{
		rb_fde_t *xF[2];
		if(rb_socketpair(AF_UNIX, SOCK_STREAM, 0, &xF[0], &xF[1], "Incoming ssld Connection") == -1)
		{
			SetIOError(new_client);
			exit_client(new_client, new_client, new_client, "Fatal Error");
			return;
		}
		new_client->localClient->ssl_callback = accept_sslcallback;
		defer = true;
		new_client->localClient->ssl_ctl = start_ssld_accept(F, xF[1], connid_get(new_client));        /* this will close F for us */
		if(new_client->localClient->ssl_ctl == NULL)
		{
			SetIOError(new_client);
			exit_client(new_client, new_client, new_client, "Service Unavailable");
			return;
		}
		F = xF[0];
		new_client->localClient->F = F;
		SetSSL(new_client);
		SetSecure(new_client);
	}
	else
	{
		struct ConfItem *aconf;
		aconf = find_conf_by_address(NULL, NULL, NULL, sai, CONF_SECURE | 1, sai->sa_family, NULL, NULL);

		if (aconf != NULL)
			SetSecure(new_client);
	}

	if (listener->wsock)
	{
		rb_fde_t *xF[2];
		if(rb_socketpair(AF_UNIX, SOCK_STREAM, 0, &xF[0], &xF[1], "Incoming wsockd Connection") == -1)
		{
			SetIOError(new_client);
			exit_client(new_client, new_client, new_client, "Fatal Error");
			return;
		}
		new_client->localClient->ws_ctl = start_wsockd_accept(F, xF[1], connid_get(new_client));        /* this will close F for us */
		if(new_client->localClient->ws_ctl == NULL)
		{
			SetIOError(new_client);
			exit_client(new_client, new_client, new_client, "Service Unavailable");
			return;
		}
		F = xF[0];
		new_client->localClient->F = F;
	}

	new_client->localClient->listener = listener;

	++listener->ref_count;

	authd_initiate_client(new_client, defer);
}

static int
accept_sslcallback(struct Client *client_p, int status)
{
	authd_deferred_client(client_p);
	return 0; /* use default handler if status != RB_OK */
}

static const char *toofast = "ERROR :Reconnecting too fast, throttled.\r\n";

static int
accept_precallback(rb_fde_t *F, struct sockaddr *addr, rb_socklen_t addrlen, void *data)
{
	struct Listener *listener = (struct Listener *)data;
	char buf[BUFSIZE];
	struct ConfItem *aconf;
	static time_t last_oper_notice = 0;
	int len;

	if(listener->ssl && (!ircd_ssl_ok || !get_ssld_count()))
	{
		rb_close(F);
		return 0;
	}

	if((maxconnections - 10) < rb_get_fd(F)) /* XXX this is kinda bogus */
	{
		++ServerStats.is_ref;
		/*
		 * slow down the whining to opers bit
		 */
		if((last_oper_notice + 20) <= rb_current_time())
		{
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
					     "All connections in use. (%s)",
					     get_listener_name(listener));
			last_oper_notice = rb_current_time();
		}

		rb_write(F, "ERROR :All connections in use\r\n", 31);
		rb_close(F);
		return 0;
	}

	aconf = find_dline(addr, addr->sa_family);
	if(aconf != NULL && (aconf->status & CONF_EXEMPTDLINE))
		return 1;

	/* Do an initial check we aren't connecting too fast or with too many
	 * from this IP... */
	if(aconf != NULL)
	{
		ServerStats.is_ref++;

		if(ConfigFileEntry.dline_with_reason)
		{
			len = snprintf(buf, sizeof(buf), "ERROR :*** Banned: %s\r\n", get_user_ban_reason(aconf));
			if (len >= (int)(sizeof(buf)-1))
			{
				buf[sizeof(buf) - 3] = '\r';
				buf[sizeof(buf) - 2] = '\n';
				buf[sizeof(buf) - 1] = '\0';
			}
		}
		else
			strcpy(buf, "ERROR :You have been D-lined.\r\n");

		rb_write(F, buf, strlen(buf));
		rb_close(F);
		return 0;
	}

	if(check_reject(F, addr)) {
		/* Reject the connection without closing the socket
		 * because it is now on the delay_exit list. */
		return 0;
	}

	if(throttle_add(addr))
	{
		rb_write(F, toofast, strlen(toofast));
		rb_close(F);
		return 0;
	}

	return 1;
}

static void
accept_callback(rb_fde_t *F, int status, struct sockaddr *addr, rb_socklen_t addrlen, void *data)
{
	struct Listener *listener = data;
	struct rb_sockaddr_storage lip;
	unsigned int locallen = sizeof(struct rb_sockaddr_storage);

	ServerStats.is_ac++;

	if(getsockname(rb_get_fd(F), (struct sockaddr *) &lip, &locallen) < 0)
	{
		/* this can fail if the connection disappeared in the meantime */
		rb_close(F);
		return;
	}

	add_connection(listener, F, addr, (struct sockaddr *)&lip);
}
