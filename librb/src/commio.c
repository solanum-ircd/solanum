/*
 *  ircd-ratbox: A slightly useful ircd.
 *  commio.c: Network/file related functions
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#include <librb_config.h>
#include <rb_lib.h>
#include <commio-int.h>
#include <commio-ssl.h>
#include <event-int.h>
#include <sys/uio.h>
#define HAVE_SSL 1

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif


struct timeout_data
{
	rb_fde_t *F;
	rb_dlink_node node;
	time_t timeout;
	PF *timeout_handler;
	void *timeout_data;
};

rb_dlink_list *rb_fd_table;
static rb_bh *fd_heap;

static rb_dlink_list timeout_list;
static rb_dlink_list closed_list;

struct defer
{
	rb_dlink_node node;
	void (*fn)(void *);
	void *data;
};
static rb_dlink_list defer_list;

static struct ev_entry *rb_timeout_ev;


static const char *rb_err_str[] = { "Comm OK", "Error during bind()",
	"Error during DNS lookup", "connect timeout",
	"Error during connect()",
	"Comm Error",
	"Error with SSL"
};

/* Highest FD and number of open FDs .. */
static int number_fd = 0;
int rb_maxconnections = 0;

static PF rb_connect_timeout;
static PF rb_connect_outcome;
static void mangle_mapped_sockaddr(struct sockaddr *in);

static inline rb_fde_t *
add_fd(int fd)
{
	rb_fde_t *F = rb_find_fd(fd);

	/* look up to see if we have it already */
	if(F != NULL)
		return F;

	F = rb_bh_alloc(fd_heap);
	F->fd = fd;
	rb_dlinkAdd(F, &F->node, &rb_fd_table[rb_hash_fd(fd)]);
	return (F);
}

static inline void
remove_fd(rb_fde_t *F)
{
	if(F == NULL || !IsFDOpen(F))
		return;

	rb_dlinkMoveNode(&F->node, &rb_fd_table[rb_hash_fd(F->fd)], &closed_list);
}

void
rb_close_pending_fds(void)
{
	rb_fde_t *F;
	rb_dlink_node *ptr, *next;
	RB_DLINK_FOREACH_SAFE(ptr, next, closed_list.head)
	{
		F = ptr->data;

		number_fd--;
		close(F->fd);
		rb_dlinkDelete(ptr, &closed_list);
		rb_bh_free(fd_heap, F);
	}
}

/* close_all_connections() can be used *before* the system come up! */

static void
rb_close_all(void)
{
	int i;

	/* XXX someone tell me why we care about 4 fd's ? */
	/* XXX btw, fd 3 is used for profiler ! */
	for(i = 3; i < rb_maxconnections; ++i)
	{
		close(i);
	}
}

/*
 * get_sockerr - get the error value from the socket or the current errno
 *
 * Get the *real* error from the socket (well try to anyway..).
 * This may only work when SO_DEBUG is enabled but its worth the
 * gamble anyway.
 */
int
rb_get_sockerr(rb_fde_t *F)
{
	int errtmp;
	int err = 0;
	rb_socklen_t len = sizeof(err);

	if(!(F->type & RB_FD_SOCKET))
		return errno;
	errtmp = errno;

#ifdef SO_ERROR
	if(F != NULL
	   && !getsockopt(rb_get_fd(F), SOL_SOCKET, SO_ERROR, (char *)&err, (rb_socklen_t *) & len))
	{
		if(err)
			errtmp = err;
	}
	errno = errtmp;
#endif
	return errtmp;
}

/*
 * rb_getmaxconnect - return the max number of connections allowed
 */
int
rb_getmaxconnect(void)
{
	return (rb_maxconnections);
}

/*
 * set_sock_buffers - set send and receive buffers for socket
 *
 * inputs	- fd file descriptor
 * 		- size to set
 * output       - returns true (1) if successful, false (0) otherwise
 * side effects -
 */
int
rb_set_buffers(rb_fde_t *F, int size)
{
	if(F == NULL)
		return 0;
	if(setsockopt
	   (F->fd, SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size))
	   || setsockopt(F->fd, SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size)))
		return 0;
	return 1;
}

/*
 * set_non_blocking - Set the client connection into non-blocking mode.
 *
 * inputs	- fd to set into non blocking mode
 * output	- 1 if successful 0 if not
 * side effects - use POSIX compliant non blocking and
 *                be done with it.
 */
int
rb_set_nb(rb_fde_t *F)
{
	int nonb = 0;
	int res;
	int fd;
	if(F == NULL)
		return 0;
	fd = F->fd;

	if((res = rb_setup_fd(F)))
		return res;
#ifdef O_NONBLOCK
	nonb |= O_NONBLOCK;
	res = fcntl(fd, F_GETFL, 0);
	if(-1 == res || fcntl(fd, F_SETFL, res | nonb) == -1)
		return 0;
#else
	nonb = 1;
	res = 0;
	if(ioctl(fd, FIONBIO, (char *)&nonb) == -1)
		return 0;
#endif

	return 1;
}

int
rb_set_cloexec(rb_fde_t *F)
{
	int res;
	int fd;
	if(F == NULL)
		return 0;
	fd = F->fd;

	res = fcntl(fd, F_GETFD, NULL);
	if(res == -1)
		return 0;
	if(fcntl(fd, F_SETFD, res | FD_CLOEXEC) == -1)
		return 0;

	return 1;
}

int
rb_clear_cloexec(rb_fde_t *F)
{
	int res;
	int fd;
	if(F == NULL)
		return 0;
	fd = F->fd;

	res = fcntl(fd, F_GETFD, NULL);
	if(res == -1)
		return 0;
	if(fcntl(fd, F_SETFD, res & ~FD_CLOEXEC) == -1)
		return 0;

	return 1;
}

/*
 * rb_settimeout() - set the socket timeout
 *
 * Set the timeout for the fd
 */
void
rb_settimeout(rb_fde_t *F, time_t timeout, PF * callback, void *cbdata)
{
	struct timeout_data *td;

	if(F == NULL)
		return;

	lrb_assert(IsFDOpen(F));
	td = F->timeout;
	if(callback == NULL)	/* user wants to remove */
	{
		if(td == NULL)
			return;
		rb_dlinkDelete(&td->node, &timeout_list);
		rb_free(td);
		F->timeout = NULL;
		if(rb_dlink_list_length(&timeout_list) == 0)
		{
			rb_event_delete(rb_timeout_ev);
			rb_timeout_ev = NULL;
		}
		return;
	}

	if(F->timeout == NULL)
		td = F->timeout = rb_malloc(sizeof(struct timeout_data));

	td->F = F;
	td->timeout = rb_current_time() + timeout;
	td->timeout_handler = callback;
	td->timeout_data = cbdata;
	rb_dlinkAdd(td, &td->node, &timeout_list);
	if(rb_timeout_ev == NULL)
	{
		rb_timeout_ev = rb_event_add("rb_checktimeouts", rb_checktimeouts, NULL, 5);
	}
}

/*
 * rb_checktimeouts() - check the socket timeouts
 *
 * All this routine does is call the given callback/cbdata, without closing
 * down the file descriptor. When close handlers have been implemented,
 * this will happen.
 */
void
rb_checktimeouts(void *notused __attribute__((unused)))
{
	rb_dlink_node *ptr, *next;
	struct timeout_data *td;
	rb_fde_t *F;
	PF *hdl;
	void *data;

	RB_DLINK_FOREACH_SAFE(ptr, next, timeout_list.head)
	{
		td = ptr->data;
		F = td->F;
		if(F == NULL || !IsFDOpen(F))
			continue;

		if(td->timeout < rb_current_time())
		{
			hdl = td->timeout_handler;
			data = td->timeout_data;
			rb_dlinkDelete(&td->node, &timeout_list);
			F->timeout = NULL;
			rb_free(td);
			hdl(F, data);
		}
	}
}

static int
rb_setsockopt_reuseaddr(rb_fde_t *F)
{
	int opt_one = 1;
	int ret;

	ret = setsockopt(F->fd, SOL_SOCKET, SO_REUSEADDR, &opt_one, sizeof(opt_one));
	if (ret) {
		rb_lib_log("rb_setsockopt_reuseaddr: Cannot set SO_REUSEADDR for FD %d: %s",
				F->fd, strerror(rb_get_sockerr(F)));
		return ret;
	}

	return 0;
}

#ifdef HAVE_LIBSCTP
static int
rb_setsockopt_sctp(rb_fde_t *F)
{
	int opt_zero = 0;
	int opt_one = 1;
	/* workaround for https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/net/sctp?id=299ee123e19889d511092347f5fc14db0f10e3a6 */
	char *env_mapped = getenv("SCTP_I_WANT_MAPPED_V4_ADDR");
	int opt_mapped = env_mapped != NULL ? atoi(env_mapped) : opt_zero;
	int ret;
	struct sctp_initmsg initmsg;
	struct sctp_rtoinfo rtoinfo;
	struct sctp_paddrparams paddrparams;
	struct sctp_assocparams assocparams;

	ret = setsockopt(F->fd, IPPROTO_SCTP, SCTP_NODELAY, &opt_one, sizeof(opt_one));
	if (ret) {
		rb_lib_log("rb_setsockopt_sctp: Cannot set SCTP_NODELAY for fd %d: %s",
				F->fd, strerror(rb_get_sockerr(F)));
		return ret;
	}

	ret = setsockopt(F->fd, IPPROTO_SCTP, SCTP_I_WANT_MAPPED_V4_ADDR, &opt_mapped, sizeof(opt_mapped));
	if (ret) {
		rb_lib_log("rb_setsockopt_sctp: Cannot unset SCTP_I_WANT_MAPPED_V4_ADDR for fd %d: %s",
				F->fd, strerror(rb_get_sockerr(F)));
		return ret;
	}

	/* Configure INIT message to specify that we only want one stream */
	memset(&initmsg, 0, sizeof(initmsg));
	initmsg.sinit_num_ostreams = 1;
	initmsg.sinit_max_instreams = 1;

	ret = setsockopt(F->fd, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg));
	if (ret) {
		rb_lib_log("rb_setsockopt_sctp: Cannot set SCTP_INITMSG for fd %d: %s",
				F->fd, strerror(rb_get_sockerr(F)));
		return ret;
	}

	/* Configure RTO values to reduce the maximum timeout */
	memset(&rtoinfo, 0, sizeof(rtoinfo));
	rtoinfo.srto_initial = 3000;
	rtoinfo.srto_min = 1000;
	rtoinfo.srto_max = 10000;

	ret = setsockopt(F->fd, IPPROTO_SCTP, SCTP_RTOINFO, &rtoinfo, sizeof(rtoinfo));
	if (ret) {
		rb_lib_log("rb_setsockopt_sctp: Cannot set SCTP_RTOINFO for fd %d: %s",
				F->fd, strerror(rb_get_sockerr(F)));
		return ret;
	}

	/*
	 * Configure peer address parameters to ensure that we monitor the connection
	 * more often than the default and don't timeout retransmit attempts before
	 * the ping timeout does.
	 *
	 * Each peer address will timeout reachability in about 750s.
	 */
	memset(&paddrparams, 0, sizeof(paddrparams));
	paddrparams.spp_assoc_id = 0;
	memcpy(&paddrparams.spp_address, &in6addr_any, sizeof(in6addr_any));
	paddrparams.spp_pathmaxrxt = 50;
	paddrparams.spp_hbinterval = 5000;
	paddrparams.spp_flags |= SPP_HB_ENABLE;

	ret = setsockopt(F->fd, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &paddrparams, sizeof(paddrparams));
	if (ret) {
		rb_lib_log("rb_setsockopt_sctp: Cannot set SCTP_PEER_ADDR_PARAMS for fd %d: %s",
				F->fd, strerror(rb_get_sockerr(F)));
		return ret;
	}

	/* Configure association parameters for retransmit attempts as above */
	memset(&assocparams, 0, sizeof(assocparams));
	assocparams.sasoc_assoc_id = 0;
	assocparams.sasoc_asocmaxrxt = 50;

	ret = setsockopt(F->fd, IPPROTO_SCTP, SCTP_ASSOCINFO, &assocparams, sizeof(assocparams));
	if (ret) {
		rb_lib_log("rb_setsockopt_sctp: Cannot set SCTP_ASSOCINFO for fd %d: %s",
				F->fd, strerror(rb_get_sockerr(F)));
		return ret;
	}

	return 0;
}
#endif

int
rb_bind(rb_fde_t *F, struct sockaddr *addr)
{
	int ret;

	ret = rb_setsockopt_reuseaddr(F);
	if (ret)
		return ret;

	ret = bind(F->fd, addr, GET_SS_LEN(addr));
	if (ret)
		return ret;

	return 0;
}

#ifdef HAVE_LIBSCTP
static int
rb_sctp_bindx_only(rb_fde_t *F, struct sockaddr_storage *addrs, size_t len)
{
	int ret;

	for (size_t i = 0; i < len; i++) {
		if (GET_SS_FAMILY(&addrs[i]) == AF_UNSPEC)
			continue;

		ret = sctp_bindx(F->fd, (struct sockaddr *)&addrs[i], 1, SCTP_BINDX_ADD_ADDR);
		if (ret)
			return ret;
	}

	return 0;
}
#endif

int
rb_sctp_bindx(rb_fde_t *F, struct sockaddr_storage *addrs, size_t len)
{
#ifdef HAVE_LIBSCTP
	int ret;

	if ((F->type & RB_FD_SCTP) == 0)
		return -1;

	ret = rb_setsockopt_reuseaddr(F);
	if (ret)
		return ret;

	ret = rb_sctp_bindx_only(F, addrs, len);
	if (ret)
		return ret;

	return 0;
#else
	return -1;
#endif
}

int
rb_inet_get_proto(rb_fde_t *F)
{
#ifdef HAVE_LIBSCTP
	if (F->type & RB_FD_SCTP)
		return IPPROTO_SCTP;
#endif
	return IPPROTO_TCP;
}

static void rb_accept_tryaccept(rb_fde_t *F, void *data __attribute__((unused))) {
	struct rb_sockaddr_storage st;
	rb_fde_t *new_F;
	rb_socklen_t addrlen;
	int new_fd;

	while(1)
	{
		memset(&st, 0, sizeof(st));
		addrlen = sizeof(st);

		new_fd = accept(F->fd, (struct sockaddr *)&st, &addrlen);
		if(new_fd < 0)
		{
			rb_setselect(F, RB_SELECT_ACCEPT, rb_accept_tryaccept, NULL);
			return;
		}

		new_F = rb_open(new_fd, RB_FD_SOCKET | (F->type & RB_FD_INHERIT_TYPES), "Incoming Connection");

		if(new_F == NULL)
		{
			rb_lib_log
				("rb_accept: new_F == NULL on incoming connection. Closing new_fd == %d",
				 new_fd);
			close(new_fd);
			continue;
		}

		if(rb_unlikely(!rb_set_nb(new_F)))
		{
			rb_lib_log("rb_accept: Couldn't set FD %d non blocking!", new_F->fd);
			rb_close(new_F);
		}

		mangle_mapped_sockaddr((struct sockaddr *)&st);

		if(F->accept->precb != NULL)
		{
			if(!F->accept->precb(new_F, (struct sockaddr *)&st, addrlen, F->accept->data))	/* pre-callback decided to drop it */
				continue;
		}
#ifdef HAVE_SSL
		if(F->type & RB_FD_SSL)
		{
			rb_ssl_accept_setup(F, new_F, (struct sockaddr *)&st, addrlen);
		}
		else
#endif /* HAVE_SSL */
		{
			F->accept->callback(new_F, RB_OK, (struct sockaddr *)&st, addrlen,
					    F->accept->data);
		}
	}

}

/* try to accept a TCP connection */
void
rb_accept_tcp(rb_fde_t *F, ACPRE * precb, ACCB * callback, void *data)
{
	if(F == NULL)
		return;
	lrb_assert(callback);

	F->accept = rb_malloc(sizeof(struct acceptdata));
	F->accept->callback = callback;
	F->accept->data = data;
	F->accept->precb = precb;
	rb_accept_tryaccept(F, NULL);
}

/*
 * void rb_connect_tcp(int fd, struct sockaddr *dest,
 *                       struct sockaddr *clocal,
 *                       CNCB *callback, void *data, int timeout)
 * Input: An fd to connect with, a host and port to connect to,
 *        a local sockaddr to connect from (or NULL to use the
 *        default), a callback, the data to pass into the callback, the
 *        address family.
 * Output: None.
 * Side-effects: A non-blocking connection to the host is started, and
 *               if necessary, set up for selection. The callback given
 *               may be called now, or it may be called later.
 */
void
rb_connect_tcp(rb_fde_t *F, struct sockaddr *dest,
	       struct sockaddr *clocal, CNCB * callback, void *data, int timeout)
{
	int retval;

	if (F == NULL)
		return;

	lrb_assert(callback);
	F->connect = rb_malloc(sizeof(struct conndata));
	F->connect->callback = callback;
	F->connect->data = data;

	memcpy(&F->connect->hostaddr, dest, sizeof(F->connect->hostaddr));

	/* Note that we're using a passed sockaddr here. This is because
	 * generally you'll be bind()ing to a sockaddr grabbed from
	 * getsockname(), so this makes things easier.
	 * XXX If NULL is passed as local, we should later on bind() to the
	 * virtual host IP, for completeness.
	 *   -- adrian
	 */
	if((clocal != NULL) && (bind(F->fd, clocal, GET_SS_LEN(clocal)) < 0))
	{
		/* Failure, call the callback with RB_ERR_BIND */
		rb_connect_callback(F, RB_ERR_BIND);
		/* ... and quit */
		return;
	}

	/* We have a valid IP, so we just call tryconnect */
	/* Make sure we actually set the timeout here .. */
	rb_settimeout(F, timeout, rb_connect_timeout, NULL);

	retval = connect(F->fd,
			 (struct sockaddr *)&F->connect->hostaddr,
			 GET_SS_LEN(&F->connect->hostaddr));
	/* Error? */
	if (retval < 0) {
		/*
		 * If we get EISCONN, then we've already connect()ed the socket,
		 * which is a good thing.
		 *   -- adrian
		 */
		if (errno == EISCONN) {
			rb_connect_callback(F, RB_OK);
		} else if (rb_ignore_errno(errno)) {
			/* Ignore error? Reschedule */
			rb_setselect(F, RB_SELECT_CONNECT, rb_connect_outcome, NULL);
		} else {
			/* Error? Fail with RB_ERR_CONNECT */
			rb_connect_callback(F, RB_ERR_CONNECT);
		}
		return;
	}
	/* If we get here, we've succeeded, so call with RB_OK */
	rb_connect_callback(F, RB_OK);
}

void
rb_connect_sctp(rb_fde_t *F, struct sockaddr_storage *dest, size_t dest_len,
	struct sockaddr_storage *clocal, size_t clocal_len,
	CNCB *callback, void *data, int timeout)
{
#ifdef HAVE_LIBSCTP
	uint8_t packed_dest[sizeof(struct sockaddr_storage) * dest_len];
	uint8_t *p = &packed_dest[0];
	size_t n = 0;
	int retval;

	if (F == NULL)
		return;

	lrb_assert(callback);
	F->connect = rb_malloc(sizeof(struct conndata));
	F->connect->callback = callback;
	F->connect->data = data;

	if ((F->type & RB_FD_SCTP) == 0) {
		rb_connect_callback(F, RB_ERR_CONNECT);
		return;
	}

	for (size_t i = 0; i < dest_len; i++) {
		if (GET_SS_FAMILY(&dest[i]) == AF_INET6) {
			memcpy(p, &dest[i], sizeof(struct sockaddr_in6));
			n++;
			p += sizeof(struct sockaddr_in6);
		} else if (GET_SS_FAMILY(&dest[i]) == AF_INET) {
			memcpy(p, &dest[i], sizeof(struct sockaddr_in));
			n++;
			p += sizeof(struct sockaddr_in);
		}
	}
	dest_len = n;

	memcpy(&F->connect->hostaddr, &dest[0], sizeof(F->connect->hostaddr));

	if ((clocal_len > 0) && (rb_sctp_bindx_only(F, clocal, clocal_len) < 0)) {
		/* Failure, call the callback with RB_ERR_BIND */
		rb_connect_callback(F, RB_ERR_BIND);
		/* ... and quit */
		return;
	}

	rb_settimeout(F, timeout, rb_connect_timeout, NULL);

	retval = sctp_connectx(F->fd, (struct sockaddr *)packed_dest, dest_len, NULL);
	/* Error? */
	if (retval < 0) {
		/*
		 * If we get EISCONN, then we've already connect()ed the socket,
		 * which is a good thing.
		 *   -- adrian
		 */
		if (errno == EISCONN) {
			rb_connect_callback(F, RB_OK);
		} else if (rb_ignore_errno(errno)) {
			/* Ignore error? Reschedule */
			rb_setselect(F, RB_SELECT_CONNECT, rb_connect_outcome, NULL);
		} else {
			/* Error? Fail with RB_ERR_CONNECT */
			rb_connect_callback(F, RB_ERR_CONNECT);
		}
		return;
	}
	/* If we get here, we've succeeded, so call with RB_OK */
	rb_connect_callback(F, RB_OK);
#else
	rb_connect_callback(F, RB_ERR_CONNECT);
#endif
}

/*
 * rb_connect_callback() - call the callback, and continue with life
 */
void
rb_connect_callback(rb_fde_t *F, int status)
{
	CNCB *hdl;
	void *data;
	int errtmp = errno;	/* save errno as rb_settimeout clobbers it sometimes */

	/* This check is gross..but probably necessary */
	if(F == NULL || F->connect == NULL || F->connect->callback == NULL)
		return;
	/* Clear the connect flag + handler */
	hdl = F->connect->callback;
	data = F->connect->data;
	F->connect->callback = NULL;


	/* Clear the timeout handler */
	rb_settimeout(F, 0, NULL, NULL);
	errno = errtmp;
	/* Call the handler */
	hdl(F, status, data);
}


/*
 * rb_connect_timeout() - this gets called when the socket connection
 * times out. This *only* can be called once connect() is initially
 * called ..
 */
static void
rb_connect_timeout(rb_fde_t *F, void *notused __attribute__((unused)))
{
	/* error! */
	rb_connect_callback(F, RB_ERR_TIMEOUT);
}

static void
rb_connect_outcome(rb_fde_t *F, void *notused __attribute__((unused)))
{
	int retval;
	int err = 0;
	socklen_t len = sizeof(err);

	if(F == NULL || F->connect == NULL || F->connect->callback == NULL)
		return;
	retval = getsockopt(F->fd, SOL_SOCKET, SO_ERROR, &err, &len);
	if ((retval >= 0) && (err != 0)) {
		errno = err;
		retval = -1;
	}
	if (retval < 0) {
		/* Error? Fail with RB_ERR_CONNECT */
		rb_connect_callback(F, RB_ERR_CONNECT);
		return;
	}
	/* If we get here, we've succeeded, so call with RB_OK */
	rb_connect_callback(F, RB_OK);
}


int
rb_connect_sockaddr(rb_fde_t *F, struct sockaddr *addr, int len)
{
	if(F == NULL)
		return 0;

	memcpy(addr, &F->connect->hostaddr, len);
	return 1;
}

/*
 * rb_error_str() - return an error string for the given error condition
 */
const char *
rb_errstr(int error)
{
	if(error < 0 || error >= RB_ERR_MAX)
		return "Invalid error number!";
	return rb_err_str[error];
}


int
rb_socketpair(int family, int sock_type, int proto, rb_fde_t **F1, rb_fde_t **F2, const char *note)
{
	int nfd[2];
	if(number_fd >= rb_maxconnections)
	{
		errno = ENFILE;
		return -1;
	}

	if(socketpair(family, sock_type, proto, nfd))
		return -1;

	*F1 = rb_open(nfd[0], RB_FD_SOCKET, note);
	*F2 = rb_open(nfd[1], RB_FD_SOCKET, note);

	if(*F1 == NULL)
	{
		if(*F2 != NULL)
			rb_close(*F2);
		return -1;
	}

	if(*F2 == NULL)
	{
		rb_close(*F1);
		return -1;
	}

	/* Set the socket non-blocking, and other wonderful bits */
	if(rb_unlikely(!rb_set_nb(*F1)))
	{
		rb_lib_log("rb_open: Couldn't set FD %d non blocking: %s", nfd[0], strerror(errno));
		rb_close(*F1);
		rb_close(*F2);
		return -1;
	}

	if(rb_unlikely(!rb_set_nb(*F2)))
	{
		rb_lib_log("rb_open: Couldn't set FD %d non blocking: %s", nfd[1], strerror(errno));
		rb_close(*F1);
		rb_close(*F2);
		return -1;
	}

	return 0;
}


int
rb_pipe(rb_fde_t **F1, rb_fde_t **F2, const char *desc)
{
	int fd[2];
	if(number_fd >= rb_maxconnections)
	{
		errno = ENFILE;
		return -1;
	}
	if(pipe(fd) == -1)
		return -1;
	*F1 = rb_open(fd[0], RB_FD_PIPE, desc);
	*F2 = rb_open(fd[1], RB_FD_PIPE, desc);

	if(rb_unlikely(!rb_set_nb(*F1)))
	{
		rb_lib_log("rb_open: Couldn't set FD %d non blocking: %s", fd[0], strerror(errno));
		rb_close(*F1);
		rb_close(*F2);
		return -1;
	}

	if(rb_unlikely(!rb_set_nb(*F2)))
	{
		rb_lib_log("rb_open: Couldn't set FD %d non blocking: %s", fd[1], strerror(errno));
		rb_close(*F1);
		rb_close(*F2);
		return -1;
	}


	return 0;
}

/*
 * rb_socket() - open a socket
 *
 * This is a highly highly cut down version of squid's rb_open() which
 * for the most part emulates socket(), *EXCEPT* it fails if we're about
 * to run out of file descriptors.
 */
rb_fde_t *
rb_socket(int family, int sock_type, int proto, const char *note)
{
	rb_fde_t *F;
	int fd;
	/* First, make sure we aren't going to run out of file descriptors */
	if(rb_unlikely(number_fd >= rb_maxconnections))
	{
		errno = ENFILE;
		return NULL;
	}

	/*
	 * Next, we try to open the socket. We *should* drop the reserved FD
	 * limit if/when we get an error, but we can deal with that later.
	 * XXX !!! -- adrian
	 */
	fd = socket(family, sock_type, proto);
	if(rb_unlikely(fd < 0))
		return NULL;	/* errno will be passed through, yay.. */

	/*
	 * Make sure we can take both IPv4 and IPv6 connections
	 * on an AF_INET6 SCTP socket, otherwise keep them separate
	 */
	if(family == AF_INET6)
	{
#ifdef HAVE_LIBSCTP
		int v6only = (proto == IPPROTO_SCTP) ? 0 : 1;
#else
		int v6only = 1;
#endif
		if(setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *) &v6only, sizeof(v6only)) == -1)
		{
			rb_lib_log("rb_socket: Could not set IPV6_V6ONLY option to %d on FD %d: %s",
				   v6only, fd, strerror(errno));
			close(fd);
			return NULL;
		}
	}

	F = rb_open(fd, RB_FD_SOCKET, note);
	if(F == NULL)
	{
		rb_lib_log("rb_socket: rb_open returns NULL on FD %d: %s, closing fd", fd,
			   strerror(errno));
		close(fd);
		return NULL;
	}

#ifdef HAVE_LIBSCTP
	if (proto == IPPROTO_SCTP) {
		F->type |= RB_FD_SCTP;

		if (rb_setsockopt_sctp(F)) {
			rb_lib_log("rb_socket: Could not set SCTP socket options on FD %d: %s",
				fd, strerror(errno));
			close(fd);
			return NULL;
		}
	}
#endif

	/* Set the socket non-blocking, and other wonderful bits */
	if(rb_unlikely(!rb_set_nb(F)))
	{
		rb_lib_log("rb_open: Couldn't set FD %d non blocking: %s", fd, strerror(errno));
		rb_close(F);
		return NULL;
	}

	return F;
}

/*
 * If a sockaddr_storage is AF_INET6 but is a mapped IPv4
 * socket manged the sockaddr.
 */
static void
mangle_mapped_sockaddr(struct sockaddr *in)
{
	struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)in;

	if(in->sa_family == AF_INET6 && IN6_IS_ADDR_V4MAPPED(&in6->sin6_addr))
	{
		struct sockaddr_in in4;
		memset(&in4, 0, sizeof(struct sockaddr_in));
		in4.sin_family = AF_INET;
		in4.sin_port = in6->sin6_port;
		in4.sin_addr.s_addr = ((uint32_t *)&in6->sin6_addr)[3];
		memcpy(in, &in4, sizeof(struct sockaddr_in));
	}
}

/*
 * rb_listen() - listen on a port
 */
int
rb_listen(rb_fde_t *F, int backlog, int defer_accept)
{
	int result;

	F->type = RB_FD_SOCKET | RB_FD_LISTEN | (F->type & RB_FD_INHERIT_TYPES);
	result = listen(F->fd, backlog);

#ifdef TCP_DEFER_ACCEPT
	if (defer_accept && !result)
	{
		(void)setsockopt(F->fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &backlog, sizeof(int));
	}
#endif
#ifdef SO_ACCEPTFILTER
	if (defer_accept && !result)
	{
		struct accept_filter_arg afa;

		memset(&afa, '\0', sizeof afa);
		rb_strlcpy(afa.af_name, "dataready", sizeof afa.af_name);
		(void)setsockopt(F->fd, SOL_SOCKET, SO_ACCEPTFILTER, &afa,
				sizeof afa);
	}
#endif

	return result;
}

void
rb_fdlist_init(int closeall, int maxfds, size_t heapsize)
{
	static int initialized = 0;

	if(!initialized)
	{
		rb_maxconnections = maxfds;
		if(closeall)
			rb_close_all();
		/* Since we're doing this once .. */
		initialized = 1;
	}
	fd_heap = rb_bh_create(sizeof(rb_fde_t), heapsize, "librb_fd_heap");

}


/* Called to open a given filedescriptor */
rb_fde_t *
rb_open(int fd, uint8_t type, const char *desc)
{
	rb_fde_t *F;
	lrb_assert(fd >= 0);

	F = add_fd(fd);

	lrb_assert(!IsFDOpen(F));
	if(rb_unlikely(IsFDOpen(F)))
	{
		const char *fdesc;
		if(F != NULL && F->desc != NULL)
			fdesc = F->desc;
		else
			fdesc = "NULL";
		rb_lib_log("Trying to rb_open an already open FD: %d desc: %s", fd, fdesc);
		return NULL;
	}
	F->fd = fd;
	F->type = type;
	SetFDOpen(F);

	if(desc != NULL)
		F->desc = rb_strndup(desc, FD_DESC_SZ);
	number_fd++;
	return F;
}


/* Called to close a given filedescriptor */
void
rb_close(rb_fde_t *F)
{
	int type, fd;

	if(F == NULL)
		return;

	fd = F->fd;
	type = F->type;
	lrb_assert(IsFDOpen(F));

	lrb_assert(!(type & RB_FD_FILE));
	if(rb_unlikely(type & RB_FD_FILE))
	{
		lrb_assert(F->read_handler == NULL);
		lrb_assert(F->write_handler == NULL);
	}

	if (type & RB_FD_LISTEN) {
		listen(F->fd, 0);
	}

	rb_setselect(F, RB_SELECT_WRITE | RB_SELECT_READ, NULL, NULL);
	rb_settimeout(F, 0, NULL, NULL);
	rb_free(F->accept);
	rb_free(F->connect);
	rb_free(F->desc);
#ifdef HAVE_SSL
	if(type & RB_FD_SSL)
	{
		rb_ssl_shutdown(F);
	}
#endif /* HAVE_SSL */
	if(IsFDOpen(F))
	{
		remove_fd(F);
		ClearFDOpen(F);
	}

	if(type & RB_FD_LISTEN)
		shutdown(fd, SHUT_RDWR);
}


/*
 * rb_dump_fd() - dump the list of active filedescriptors
 */
void
rb_dump_fd(DUMPCB * cb, void *data)
{
	static const char *empty = "";
	rb_dlink_node *ptr;
	rb_dlink_list *bucket;
	rb_fde_t *F;
	unsigned int i;

	for(i = 0; i < RB_FD_HASH_SIZE; i++)
	{
		bucket = &rb_fd_table[i];

		if(rb_dlink_list_length(bucket) <= 0)
			continue;

		RB_DLINK_FOREACH(ptr, bucket->head)
		{
			F = ptr->data;
			if(F == NULL || !IsFDOpen(F))
				continue;

			cb(F->fd, F->desc ? F->desc : empty, data);
		}
	}
}

/*
 * rb_note() - set the fd note
 *
 * Note: must be careful not to overflow rb_fd_table[fd].desc when
 *       calling.
 */
void
rb_note(rb_fde_t *F, const char *string)
{
	if(F == NULL)
		return;

	rb_free(F->desc);
	F->desc = rb_strndup(string, FD_DESC_SZ);
}

void
rb_set_type(rb_fde_t *F, uint8_t type)
{
	/* if the caller is calling this, lets assume they have a clue */
	F->type = type;
	return;
}

uint8_t
rb_get_type(rb_fde_t *F)
{
	return F->type;
}

int
rb_fd_ssl(rb_fde_t *F)
{
	if(F == NULL)
		return 0;
	if(F->type & RB_FD_SSL)
		return 1;
	return 0;
}

int
rb_get_fd(rb_fde_t *F)
{
	if(F == NULL)
		return -1;
	return (F->fd);
}

rb_fde_t *
rb_get_fde(int fd)
{
	return rb_find_fd(fd);
}

ssize_t
rb_read(rb_fde_t *F, void *buf, int count)
{
	ssize_t ret;
	if(F == NULL)
		return 0;

	/* This needs to be *before* RB_FD_SOCKET otherwise you'll process
	 * an SSL socket as a regular socket
	 */
#ifdef HAVE_SSL
	if(F->type & RB_FD_SSL)
	{
		return rb_ssl_read(F, buf, count);
	}
#endif
	if(F->type & RB_FD_SOCKET)
	{
		ret = recv(F->fd, buf, count, 0);
		return ret;
	}


	/* default case */
	return read(F->fd, buf, count);
}


ssize_t
rb_write(rb_fde_t *F, const void *buf, int count)
{
	ssize_t ret;
	if(F == NULL)
		return 0;

#ifdef HAVE_SSL
	if(F->type & RB_FD_SSL)
	{
		return rb_ssl_write(F, buf, count);
	}
#endif
	if(F->type & RB_FD_SOCKET)
	{
		ret = send(F->fd, buf, count, MSG_NOSIGNAL);
		return ret;
	}

	return write(F->fd, buf, count);
}

#ifdef HAVE_SSL
static ssize_t
rb_fake_writev(rb_fde_t *F, const struct rb_iovec *vp, size_t vpcount)
{
	ssize_t count = 0;

	while(vpcount-- > 0)
	{
		ssize_t written = rb_write(F, vp->iov_base, vp->iov_len);

		if(written <= 0)
		{
			if(count > 0)
				return count;
			else
				return written;
		}
		count += written;
		vp++;
	}
	return (count);
}
#endif

ssize_t
rb_writev(rb_fde_t *F, struct rb_iovec * vector, int count)
{
	if(F == NULL)
	{
		errno = EBADF;
		return -1;
	}
#ifdef HAVE_SSL
	if(F->type & RB_FD_SSL)
	{
		return rb_fake_writev(F, vector, count);
	}
#endif /* HAVE_SSL */
	if(F->type & RB_FD_SOCKET)
	{
		struct msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = (struct iovec *)vector;
		msg.msg_iovlen = count;
		return sendmsg(F->fd, &msg, MSG_NOSIGNAL);
	}
	return writev(F->fd, (struct iovec *)vector, count);

}

/*
 * From: Thomas Helvey <tomh@inxpress.net>
 */
static const char *IpQuadTab[] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
	"10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
	"20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
	"30", "31", "32", "33", "34", "35", "36", "37", "38", "39",
	"40", "41", "42", "43", "44", "45", "46", "47", "48", "49",
	"50", "51", "52", "53", "54", "55", "56", "57", "58", "59",
	"60", "61", "62", "63", "64", "65", "66", "67", "68", "69",
	"70", "71", "72", "73", "74", "75", "76", "77", "78", "79",
	"80", "81", "82", "83", "84", "85", "86", "87", "88", "89",
	"90", "91", "92", "93", "94", "95", "96", "97", "98", "99",
	"100", "101", "102", "103", "104", "105", "106", "107", "108", "109",
	"110", "111", "112", "113", "114", "115", "116", "117", "118", "119",
	"120", "121", "122", "123", "124", "125", "126", "127", "128", "129",
	"130", "131", "132", "133", "134", "135", "136", "137", "138", "139",
	"140", "141", "142", "143", "144", "145", "146", "147", "148", "149",
	"150", "151", "152", "153", "154", "155", "156", "157", "158", "159",
	"160", "161", "162", "163", "164", "165", "166", "167", "168", "169",
	"170", "171", "172", "173", "174", "175", "176", "177", "178", "179",
	"180", "181", "182", "183", "184", "185", "186", "187", "188", "189",
	"190", "191", "192", "193", "194", "195", "196", "197", "198", "199",
	"200", "201", "202", "203", "204", "205", "206", "207", "208", "209",
	"210", "211", "212", "213", "214", "215", "216", "217", "218", "219",
	"220", "221", "222", "223", "224", "225", "226", "227", "228", "229",
	"230", "231", "232", "233", "234", "235", "236", "237", "238", "239",
	"240", "241", "242", "243", "244", "245", "246", "247", "248", "249",
	"250", "251", "252", "253", "254", "255"
};

/*
 * inetntoa - in_addr to string
 *      changed name to remove collision possibility and
 *      so behaviour is guaranteed to take a pointer arg.
 *      -avalon 23/11/92
 *  inet_ntoa --  returned the dotted notation of a given
 *      internet number
 *      argv 11/90).
 *  inet_ntoa --  its broken on some Ultrix/Dynix too. -avalon
 */

static const char *
inetntoa(const char *in)
{
	static char buf[16];
	char *bufptr = buf;
	const unsigned char *a = (const unsigned char *)in;
	const char *n;

	n = IpQuadTab[*a++];
	while(*n)
		*bufptr++ = *n++;
	*bufptr++ = '.';
	n = IpQuadTab[*a++];
	while(*n)
		*bufptr++ = *n++;
	*bufptr++ = '.';
	n = IpQuadTab[*a++];
	while(*n)
		*bufptr++ = *n++;
	*bufptr++ = '.';
	n = IpQuadTab[*a];
	while(*n)
		*bufptr++ = *n++;
	*bufptr = '\0';
	return buf;
}

/*
 * WARNING: Don't even consider trying to compile this on a system where
 * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
 */

static const char *inet_ntop4(const unsigned char *src, char *dst, unsigned int size);
static const char *inet_ntop6(const unsigned char *src, char *dst, unsigned int size);

/* const char *
 * inet_ntop4(src, dst, size)
 *	format an IPv4 address
 * return:
 *	`dst' (as a const)
 * notes:
 *	(1) uses no statics
 *	(2) takes a unsigned char* not an in_addr as input
 * author:
 *	Paul Vixie, 1996.
 */
static const char *
inet_ntop4(const unsigned char *src, char *dst, unsigned int size)
{
	if(size < 16)
		return NULL;
	return strcpy(dst, inetntoa((const char *)src));
}

/* const char *
 * inet_ntop6(src, dst, size)
 *	convert IPv6 binary address into presentation (printable) format
 * author:
 *	Paul Vixie, 1996.
 */
static const char *
inet_ntop6(const unsigned char *src, char *dst, unsigned int size)
{
	/*
	 * Note that int32_t and int16_t need only be "at least" large enough
	 * to contain a value of the specified size.  On some systems, like
	 * Crays, there is no such thing as an integer variable with 16 bits.
	 * Keep this in mind if you think this function should have been coded
	 * to use pointer overlays.  All the world's not a VAX.
	 */
	char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
	struct
	{
		int base, len;
	}
	best, cur;
	unsigned int words[IN6ADDRSZ / INT16SZ];
	int i;

	/*
	 * Preprocess:
	 *      Copy the input (bytewise) array into a wordwise array.
	 *      Find the longest run of 0x00's in src[] for :: shorthanding.
	 */
	memset(words, '\0', sizeof words);
	for(i = 0; i < IN6ADDRSZ; i += 2)
		words[i / 2] = (src[i] << 8) | src[i + 1];
	best.base = -1;
	best.len = 0;
	cur.base = -1;
	cur.len = 0;
	for(i = 0; i < (IN6ADDRSZ / INT16SZ); i++)
	{
		if(words[i] == 0)
		{
			if(cur.base == -1)
				cur.base = i, cur.len = 1;
			else
				cur.len++;
		}
		else
		{
			if(cur.base != -1)
			{
				if(best.base == -1 || cur.len > best.len)
					best = cur;
				cur.base = -1;
			}
		}
	}
	if(cur.base != -1)
	{
		if(best.base == -1 || cur.len > best.len)
			best = cur;
	}
	if(best.base != -1 && best.len < 2)
		best.base = -1;

	/*
	 * Format the result.
	 */
	tp = tmp;
	for(i = 0; i < (IN6ADDRSZ / INT16SZ); i++)
	{
		/* Are we inside the best run of 0x00's? */
		if(best.base != -1 && i >= best.base && i < (best.base + best.len))
		{
			if(i == best.base)
			{
				if(i == 0)
					*tp++ = '0';
				*tp++ = ':';
			}
			continue;
		}
		/* Are we following an initial run of 0x00s or any real hex? */
		if(i != 0)
			*tp++ = ':';
		/* Is this address an encapsulated IPv4? */
		if(i == 6 && best.base == 0 &&
		   (best.len == 6 || (best.len == 5 && words[5] == 0xffff)))
		{
			if(!inet_ntop4(src + 12, tp, sizeof tmp - (tp - tmp)))
				return (NULL);
			tp += strlen(tp);
			break;
		}
		tp += sprintf(tp, "%x", words[i]);
	}
	/* Was it a trailing run of 0x00's? */
	if(best.base != -1 && (best.base + best.len) == (IN6ADDRSZ / INT16SZ))
		*tp++ = ':';
	*tp++ = '\0';

	/*
	 * Check for overflow, copy, and we're done.
	 */

	if((unsigned int)(tp - tmp) > size)
	{
		return (NULL);
	}
	return memcpy(dst, tmp, tp - tmp);
}

int
rb_inet_pton_sock(const char *src, struct sockaddr_storage *dst)
{
	memset(dst, 0, sizeof(*dst));
	if(rb_inet_pton(AF_INET, src, &((struct sockaddr_in *)dst)->sin_addr))
	{
		SET_SS_FAMILY(dst, AF_INET);
		SET_SS_PORT(dst, 0);
		SET_SS_LEN(dst, sizeof(struct sockaddr_in));
		return 1;
	}
	else if(rb_inet_pton(AF_INET6, src, &((struct sockaddr_in6 *)dst)->sin6_addr))
	{
		SET_SS_FAMILY(dst, AF_INET6);
		SET_SS_PORT(dst, 0);
		SET_SS_LEN(dst, sizeof(struct sockaddr_in6));
		return 1;
	}
	return 0;
}

const char *
rb_inet_ntop_sock(struct sockaddr *src, char *dst, unsigned int size)
{
	switch (src->sa_family)
	{
	case AF_INET:
		return (rb_inet_ntop(AF_INET, &((struct sockaddr_in *)src)->sin_addr, dst, size));
	case AF_INET6:
		return (rb_inet_ntop
			(AF_INET6, &((struct sockaddr_in6 *)src)->sin6_addr, dst, size));
	default:
		return NULL;
	}
}

/* char *
 * rb_inet_ntop(af, src, dst, size)
 *	convert a network format address to presentation format.
 * return:
 *	pointer to presentation format address (`dst'), or NULL (see errno).
 * author:
 *	Paul Vixie, 1996.
 */
const char *
rb_inet_ntop(int af, const void *src, char *dst, unsigned int size)
{
	switch (af)
	{
	case AF_INET:
		return (inet_ntop4(src, dst, size));
	case AF_INET6:
		if(IN6_IS_ADDR_V4MAPPED((const struct in6_addr *)src) ||
		   IN6_IS_ADDR_V4COMPAT((const struct in6_addr *)src))
			return (inet_ntop4
				((const unsigned char *)&((const struct in6_addr *)src)->
				 s6_addr[12], dst, size));
		else
			return (inet_ntop6(src, dst, size));
	default:
		return (NULL);
	}
	/* NOTREACHED */
}

/*
 * WARNING: Don't even consider trying to compile this on a system where
 * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
 */

/* int
 * rb_inet_pton(af, src, dst)
 *	convert from presentation format (which usually means ASCII printable)
 *	to network format (which is usually some kind of binary format).
 * return:
 *	1 if the address was valid for the specified address family
 *	0 if the address wasn't valid (`dst' is untouched in this case)
 *	-1 if some other error occurred (`dst' is untouched in this case, too)
 * author:
 *	Paul Vixie, 1996.
 */

/* int
 * inet_pton4(src, dst)
 *	like inet_aton() but without all the hexadecimal and shorthand.
 * return:
 *	1 if `src' is a valid dotted quad, else 0.
 * notice:
 *	does not touch `dst' unless it's returning 1.
 * author:
 *	Paul Vixie, 1996.
 */
static int
inet_pton4(const char *src, unsigned char *dst)
{
	int saw_digit, octets, ch;
	unsigned char tmp[INADDRSZ], *tp;

	saw_digit = 0;
	octets = 0;
	*(tp = tmp) = 0;
	while((ch = *src++) != '\0')
	{

		if(ch >= '0' && ch <= '9')
		{
			unsigned int new = *tp * 10 + (ch - '0');

			if(new > 255)
				return (0);
			*tp = new;
			if(!saw_digit)
			{
				if(++octets > 4)
					return (0);
				saw_digit = 1;
			}
		}
		else if(ch == '.' && saw_digit)
		{
			if(octets == 4)
				return (0);
			*++tp = 0;
			saw_digit = 0;
		}
		else
			return (0);
	}
	if(octets < 4)
		return (0);
	memcpy(dst, tmp, INADDRSZ);
	return (1);
}

/* int
 * inet_pton6(src, dst)
 *	convert presentation level address to network order binary form.
 * return:
 *	1 if `src' is a valid [RFC1884 2.2] address, else 0.
 * notice:
 *	(1) does not touch `dst' unless it's returning 1.
 *	(2) :: in a full address is silently ignored.
 * credit:
 *	inspired by Mark Andrews.
 * author:
 *	Paul Vixie, 1996.
 */

static int
inet_pton6(const char *src, unsigned char *dst)
{
	static const char xdigits[] = "0123456789abcdef";
	unsigned char tmp[IN6ADDRSZ], *tp, *endp, *colonp;
	const char *curtok;
	int ch, saw_xdigit;
	unsigned int val;

	tp = memset(tmp, '\0', IN6ADDRSZ);
	endp = tp + IN6ADDRSZ;
	colonp = NULL;
	/* Leading :: requires some special handling. */
	if(*src == ':')
		if(*++src != ':')
			return (0);
	curtok = src;
	saw_xdigit = 0;
	val = 0;
	while((ch = tolower((unsigned char)*src++)) != '\0')
	{
		const char *pch;

		pch = strchr(xdigits, ch);
		if(pch != NULL)
		{
			val <<= 4;
			val |= (pch - xdigits);
			if(val > 0xffff)
				return (0);
			saw_xdigit = 1;
			continue;
		}
		if(ch == ':')
		{
			curtok = src;
			if(!saw_xdigit)
			{
				if(colonp)
					return (0);
				colonp = tp;
				continue;
			}
			else if(*src == '\0')
			{
				return (0);
			}
			if(tp + INT16SZ > endp)
				return (0);
			*tp++ = (unsigned char)(val >> 8) & 0xff;
			*tp++ = (unsigned char)val & 0xff;
			saw_xdigit = 0;
			val = 0;
			continue;
		}
		if(*src != '\0' && ch == '.')
		{
			if(((tp + INADDRSZ) <= endp) && inet_pton4(curtok, tp) > 0)
			{
				tp += INADDRSZ;
				saw_xdigit = 0;
				break;	/* '\0' was seen by inet_pton4(). */
			}
		}
		else
			continue;
		return (0);
	}
	if(saw_xdigit)
	{
		if(tp + INT16SZ > endp)
			return (0);
		*tp++ = (unsigned char)(val >> 8) & 0xff;
		*tp++ = (unsigned char)val & 0xff;
	}
	if(colonp != NULL)
	{
		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		const int n = tp - colonp;
		int i;

		if(tp == endp)
			return (0);
		for(i = 1; i <= n; i++)
		{
			endp[-i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}
	if(tp != endp)
		return (0);
	memcpy(dst, tmp, IN6ADDRSZ);
	return (1);
}

int
rb_inet_pton(int af, const char *src, void *dst)
{
	switch (af)
	{
	case AF_INET:
		return (inet_pton4(src, dst));
	case AF_INET6:
		/* Somebody might have passed as an IPv4 address this is sick but it works */
		if(inet_pton4(src, dst))
		{
			char tmp[HOSTIPLEN];
			sprintf(tmp, "::ffff:%s", src);
			return (inet_pton6(tmp, dst));
		}
		else
			return (inet_pton6(src, dst));
	default:
		return (-1);
	}
	/* NOTREACHED */
}


static void (*setselect_handler) (rb_fde_t *, unsigned int, PF *, void *);
static int (*select_handler) (long);
static int (*setup_fd_handler) (rb_fde_t *);
static int (*io_sched_event) (struct ev_entry *, int);
static void (*io_unsched_event) (struct ev_entry *);
static int (*io_supports_event) (void);
static void (*io_init_event) (void);
static char iotype[25];

const char *
rb_get_iotype(void)
{
	return iotype;
}

static int
rb_unsupported_event(void)
{
	return 0;
}

static int
try_kqueue(void)
{
	if(!rb_init_netio_kqueue())
	{
		setselect_handler = rb_setselect_kqueue;
		select_handler = rb_select_kqueue;
		setup_fd_handler = rb_setup_fd_kqueue;
		io_sched_event = rb_kqueue_sched_event;
		io_unsched_event = rb_kqueue_unsched_event;
		io_init_event = rb_kqueue_init_event;
		io_supports_event = rb_kqueue_supports_event;
		rb_strlcpy(iotype, "kqueue", sizeof(iotype));
		return 0;
	}
	return -1;
}

static int
try_epoll(void)
{
	if(!rb_init_netio_epoll())
	{
		setselect_handler = rb_setselect_epoll;
		select_handler = rb_select_epoll;
		setup_fd_handler = rb_setup_fd_epoll;
		io_sched_event = rb_epoll_sched_event;
		io_unsched_event = rb_epoll_unsched_event;
		io_supports_event = rb_epoll_supports_event;
		io_init_event = rb_epoll_init_event;
		rb_strlcpy(iotype, "epoll", sizeof(iotype));
		return 0;
	}
	return -1;
}

static int
try_ports(void)
{
	if(!rb_init_netio_ports())
	{
		setselect_handler = rb_setselect_ports;
		select_handler = rb_select_ports;
		setup_fd_handler = rb_setup_fd_ports;
		io_sched_event = rb_ports_sched_event;
		io_unsched_event = rb_ports_unsched_event;
		io_init_event =  rb_ports_init_event;
		io_supports_event = rb_ports_supports_event;
		rb_strlcpy(iotype, "ports", sizeof(iotype));
		return 0;
	}
	return -1;
}

static int
try_devpoll(void)
{
	if(!rb_init_netio_devpoll())
	{
		setselect_handler = rb_setselect_devpoll;
		select_handler = rb_select_devpoll;
		setup_fd_handler = rb_setup_fd_devpoll;
		io_sched_event = NULL;
		io_unsched_event = NULL;
		io_init_event = NULL;
		io_supports_event = rb_unsupported_event;
		rb_strlcpy(iotype, "devpoll", sizeof(iotype));
		return 0;
	}
	return -1;
}

static int
try_sigio(void)
{
	if(!rb_init_netio_sigio())
	{
		setselect_handler = rb_setselect_sigio;
		select_handler = rb_select_sigio;
		setup_fd_handler = rb_setup_fd_sigio;
		io_sched_event = rb_sigio_sched_event;
		io_unsched_event = rb_sigio_unsched_event;
		io_supports_event = rb_sigio_supports_event;
		io_init_event = rb_sigio_init_event;

		rb_strlcpy(iotype, "sigio", sizeof(iotype));
		return 0;
	}
	return -1;
}

static int
try_poll(void)
{
	if(!rb_init_netio_poll())
	{
		setselect_handler = rb_setselect_poll;
		select_handler = rb_select_poll;
		setup_fd_handler = rb_setup_fd_poll;
		io_sched_event = NULL;
		io_unsched_event = NULL;
		io_init_event = NULL;
		io_supports_event = rb_unsupported_event;
		rb_strlcpy(iotype, "poll", sizeof(iotype));
		return 0;
	}
	return -1;
}

int
rb_io_sched_event(struct ev_entry *ev, int when)
{
	if(ev == NULL || io_supports_event == NULL || io_sched_event == NULL
	   || !io_supports_event())
		return 0;
	return io_sched_event(ev, when);
}

void
rb_io_unsched_event(struct ev_entry *ev)
{
	if(ev == NULL || io_supports_event == NULL || io_unsched_event == NULL
	   || !io_supports_event())
		return;
	io_unsched_event(ev);
}

int
rb_io_supports_event(void)
{
	if(io_supports_event == NULL)
		return 0;
	return io_supports_event();
}

void
rb_io_init_event(void)
{
	io_init_event();
	rb_event_io_register_all();
}

void
rb_init_netio(void)
{
	char *ioenv = getenv("LIBRB_USE_IOTYPE");
	rb_fd_table = rb_malloc(RB_FD_HASH_SIZE * sizeof(rb_dlink_list));
	rb_init_ssl();

	if(ioenv != NULL)
	{
		if(!strcmp("epoll", ioenv))
		{
			if(!try_epoll())
				return;
		}
		else if(!strcmp("kqueue", ioenv))
		{
			if(!try_kqueue())
				return;
		}
		else if(!strcmp("ports", ioenv))
		{
			if(!try_ports())
				return;
		}
		else if(!strcmp("poll", ioenv))
		{
			if(!try_poll())
				return;
		}
		else if(!strcmp("devpoll", ioenv))
		{
			if(!try_devpoll())
				return;
		}
		else if(!strcmp("sigio", ioenv))
		{
			if(!try_sigio())
				return;
		}
	}

	if(!try_kqueue())
		return;
	if(!try_epoll())
		return;
	if(!try_ports())
		return;
	if(!try_devpoll())
		return;
	if(!try_sigio())
		return;
	if(!try_poll())
		return;

	rb_lib_log("rb_init_netio: Could not find any io handlers...giving up");

	abort();
}

void
rb_setselect(rb_fde_t *F, unsigned int type, PF * handler, void *client_data)
{
	setselect_handler(F, type, handler, client_data);
}

void
rb_defer(void (*fn)(void *), void *data)
{
	struct defer *defer = rb_malloc(sizeof *defer);
	defer->fn = fn;
	defer->data = data;
	rb_dlinkAdd(defer, &defer->node, &defer_list);
}

int
rb_select(unsigned long timeout)
{
	int ret = select_handler(timeout);
	rb_dlink_node *ptr, *next;
	RB_DLINK_FOREACH_SAFE(ptr, next, defer_list.head)
	{
		struct defer *defer = ptr->data;
		defer->fn(defer->data);
		rb_dlinkDelete(ptr, &defer_list);
		rb_free(defer);
	}
	rb_close_pending_fds();
	return ret;
}

int
rb_setup_fd(rb_fde_t *F)
{
	rb_set_cloexec(F);
	return setup_fd_handler(F);
}


int
rb_ignore_errno(int error)
{
	switch (error)
	{
#ifdef EINPROGRESS
	case EINPROGRESS:
#endif
#if defined EWOULDBLOCK
	case EWOULDBLOCK:
#endif
#if defined(EAGAIN) && (EWOULDBLOCK != EAGAIN)
	case EAGAIN:
#endif
#ifdef EINTR
	case EINTR:
#endif
#ifdef ERESTART
	case ERESTART:
#endif
#ifdef ENOBUFS
	case ENOBUFS:
#endif
		return 1;
	default:
		break;
	}
	return 0;
}


int
rb_recv_fd_buf(rb_fde_t *F, void *data, size_t datasize, rb_fde_t **xF, int nfds)
{
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov[1];
	struct stat st;
	uint8_t stype = RB_FD_UNKNOWN;
	const char *desc;
	int fd, len, x, rfds;

	int control_len = CMSG_SPACE(sizeof(int) * nfds);

	iov[0].iov_base = data;
	iov[0].iov_len = datasize;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_flags = 0;
	cmsg = alloca(control_len);
	msg.msg_control = cmsg;
	msg.msg_controllen = control_len;

	if((len = recvmsg(rb_get_fd(F), &msg, 0)) <= 0)
		return len;

	if(msg.msg_controllen > 0 && msg.msg_control != NULL
	   && (cmsg = CMSG_FIRSTHDR(&msg)) != NULL)
	{
		rfds = ((unsigned char *)cmsg + cmsg->cmsg_len - CMSG_DATA(cmsg)) / sizeof(int);

		for(x = 0; x < nfds && x < rfds; x++)
		{
			fd = ((int *)CMSG_DATA(cmsg))[x];
			stype = RB_FD_UNKNOWN;
			desc = "remote unknown";
			if(!fstat(fd, &st))
			{
				if(S_ISSOCK(st.st_mode))
				{
					stype = RB_FD_SOCKET;
					desc = "remote socket";
				}
				else if(S_ISFIFO(st.st_mode))
				{
					stype = RB_FD_PIPE;
					desc = "remote pipe";
				}
				else if(S_ISREG(st.st_mode))
				{
					stype = RB_FD_FILE;
					desc = "remote file";
				}
			}
			xF[x] = rb_open(fd, stype, desc);
		}
	}
	else
		*xF = NULL;
	return len;
}


int
rb_send_fd_buf(rb_fde_t *xF, rb_fde_t **F, int count, void *data, size_t datasize, pid_t pid __attribute__((unused)))
{
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov[1];
	char empty = '0';

	memset(&msg, 0, sizeof(msg));
	if(datasize == 0)
	{
		iov[0].iov_base = &empty;
		iov[0].iov_len = 1;
	}
	else
	{
		iov[0].iov_base = data;
		iov[0].iov_len = datasize;
	}
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_flags = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;

	if(count > 0)
	{
		size_t ucount = (size_t)count;
		int len = CMSG_SPACE(sizeof(int) * count);
		char buf[len];

		msg.msg_control = buf;
		msg.msg_controllen = len;
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int) * count);

		for(size_t i = 0; i < ucount; i++)
		{
			((int *)CMSG_DATA(cmsg))[i] = rb_get_fd(F[i]);
		}
		msg.msg_controllen = cmsg->cmsg_len;
		return sendmsg(rb_get_fd(xF), &msg, MSG_NOSIGNAL);
	}
	return sendmsg(rb_get_fd(xF), &msg, MSG_NOSIGNAL);
}

int
rb_ipv4_from_ipv6(const struct sockaddr_in6 *restrict ip6, struct sockaddr_in *restrict ip4)
{
	int i;

	if (!memcmp(ip6->sin6_addr.s6_addr, "\x20\x02", 2))
	{
		/* 6to4 and similar */
		memcpy(&ip4->sin_addr, ip6->sin6_addr.s6_addr + 2, 4);
	}
	else if (!memcmp(ip6->sin6_addr.s6_addr, "\x20\x01\x00\x00", 4))
	{
		/* Teredo */
		for (i = 0; i < 4; i++)
			((uint8_t *)&ip4->sin_addr)[i] = 0xFF ^
				ip6->sin6_addr.s6_addr[12 + i];
	}
	else
		return 0;
	SET_SS_LEN(ip4, sizeof(struct sockaddr_in));
	ip4->sin_family = AF_INET;
	ip4->sin_port = 0;
	return 1;
}
