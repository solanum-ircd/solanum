/*
 *  librb: a library used by ircd-ratbox and other things
 *  openssl.c: OpenSSL backend
 *
 *  Copyright (C) 2007-2008 ircd-ratbox development team
 *  Copyright (C) 2007-2008 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2015-2016 Aaron Jones <aaronmdjones@gmail.com>
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

#ifdef HAVE_OPENSSL

#include <commio-int.h>
#include <commio-ssl.h>

#include "openssl_ratbox.h"

typedef enum
{
	RB_FD_TLS_DIRECTION_IN = 0,
	RB_FD_TLS_DIRECTION_OUT = 1
} rb_fd_tls_direction;

#define SSL_P(x) ((SSL *)((x)->ssl))



static SSL_CTX *ssl_ctx = NULL;

struct ssl_connect
{
	CNCB *callback;
	void *data;
	int timeout;
};

static const char *rb_ssl_strerror(unsigned long);
static void rb_ssl_connect_realcb(rb_fde_t *, int, struct ssl_connect *);



/*
 * Internal OpenSSL-specific code
 */

static unsigned long
rb_ssl_last_err(void)
{
	unsigned long err_saved, err = 0;

	while((err_saved = ERR_get_error()) != 0)
		err = err_saved;

	return err;
}

static void
rb_ssl_init_fd(rb_fde_t *const F, const rb_fd_tls_direction dir)
{
	(void) rb_ssl_last_err();

	F->ssl = SSL_new(ssl_ctx);

	if(F->ssl == NULL)
	{
		rb_lib_log("%s: SSL_new: %s", __func__, rb_ssl_strerror(rb_ssl_last_err()));
		rb_close(F);
		return;
	}

	switch(dir)
	{
	case RB_FD_TLS_DIRECTION_IN:
		SSL_set_accept_state(SSL_P(F));
		break;
	case RB_FD_TLS_DIRECTION_OUT:
		SSL_set_connect_state(SSL_P(F));
		break;
	}

	SSL_set_fd(SSL_P(F), rb_get_fd(F));
}

static void
rb_ssl_accept_common(rb_fde_t *const F, void *const data __attribute__((unused)))
{
	lrb_assert(F != NULL);
	lrb_assert(F->accept != NULL);
	lrb_assert(F->accept->callback != NULL);
	lrb_assert(F->ssl != NULL);

	(void) rb_ssl_last_err();

	int ret = SSL_do_handshake(SSL_P(F));
	int err = SSL_get_error(SSL_P(F), ret);

	if(ret == 1)
	{
		F->handshake_count++;

		rb_settimeout(F, 0, NULL, NULL);
		rb_setselect(F, RB_SELECT_READ | RB_SELECT_WRITE, NULL, NULL);

		struct acceptdata *const ad = F->accept;
		F->accept = NULL;
		ad->callback(F, RB_OK, (struct sockaddr *)&ad->S, ad->addrlen, ad->data);
		rb_free(ad);

		return;
	}
	if(ret == -1 && err == SSL_ERROR_WANT_READ)
	{
		rb_setselect(F, RB_SELECT_READ, rb_ssl_accept_common, NULL);
		return;
	}
	if(ret == -1 && err == SSL_ERROR_WANT_WRITE)
	{
		rb_setselect(F, RB_SELECT_WRITE, rb_ssl_accept_common, NULL);
		return;
	}

	errno = EIO;
	F->ssl_errno = (unsigned long) err;
	F->accept->callback(F, RB_ERROR_SSL, NULL, 0, F->accept->data);
}

static void
rb_ssl_connect_common(rb_fde_t *const F, void *const data)
{
	lrb_assert(F != NULL);
	lrb_assert(F->ssl != NULL);

	(void) rb_ssl_last_err();

	int ret = SSL_do_handshake(SSL_P(F));
	int err = SSL_get_error(SSL_P(F), ret);

	if(ret == 1)
	{
		F->handshake_count++;

		rb_settimeout(F, 0, NULL, NULL);
		rb_setselect(F, RB_SELECT_READ | RB_SELECT_WRITE, NULL, NULL);

		rb_ssl_connect_realcb(F, RB_OK, data);

		return;
	}
	if(ret == -1 && err == SSL_ERROR_WANT_READ)
	{
		rb_setselect(F, RB_SELECT_READ, rb_ssl_connect_common, data);
		return;
	}
	if(ret == -1 && err == SSL_ERROR_WANT_WRITE)
	{
		rb_setselect(F, RB_SELECT_WRITE, rb_ssl_connect_common, data);
		return;
	}

	errno = EIO;
	F->ssl_errno = (unsigned long) err;
	rb_ssl_connect_realcb(F, RB_ERROR_SSL, data);
}

static const char *
rb_ssl_strerror(const unsigned long err)
{
	static char errbuf[512];

	ERR_error_string_n(err, errbuf, sizeof errbuf);

	return errbuf;
}

static int
verify_accept_all_cb(const int preverify_ok __attribute__((unused)), X509_STORE_CTX *const x509_ctx __attribute__((unused)))
{
	return 1;
}

static ssize_t
rb_ssl_read_or_write(const int r_or_w, rb_fde_t *const F, void *const rbuf, const void *const wbuf, const size_t count)
{
	ssize_t ret;
	unsigned long err;

	(void) rb_ssl_last_err();

	if(r_or_w == 0)
		ret = (ssize_t) SSL_read(SSL_P(F), rbuf, (int)count);
	else
		ret = (ssize_t) SSL_write(SSL_P(F), wbuf, (int)count);

	if(ret < 0)
	{
		switch(SSL_get_error(SSL_P(F), ret))
		{
		case SSL_ERROR_WANT_READ:
			errno = EAGAIN;
			return RB_RW_SSL_NEED_READ;
		case SSL_ERROR_WANT_WRITE:
			errno = EAGAIN;
			return RB_RW_SSL_NEED_WRITE;
		case SSL_ERROR_ZERO_RETURN:
			return 0;
		case SSL_ERROR_SYSCALL:
			err = rb_ssl_last_err();
			if(err == 0)
			{
				F->ssl_errno = 0;
				return RB_RW_IO_ERROR;
			}
			break;
		default:
			err = rb_ssl_last_err();
			break;
		}

		F->ssl_errno = err;
		if(err > 0)
		{
			errno = EIO;	/* not great but... */
			return RB_RW_SSL_ERROR;
		}
		return RB_RW_IO_ERROR;
	}
	return ret;
}

static int
make_certfp(X509 *const cert, uint8_t certfp[const RB_SSL_CERTFP_LEN], const int method)
{
	unsigned int hashlen = 0;
	const EVP_MD *md_type = NULL;
	const ASN1_ITEM *item = NULL;
	void *data = NULL;

	switch(method)
	{
	case RB_SSL_CERTFP_METH_CERT_SHA1:
		hashlen = RB_SSL_CERTFP_LEN_SHA1;
		md_type = EVP_sha1();
		item = ASN1_ITEM_rptr(X509);
		data = cert;
		break;
	case RB_SSL_CERTFP_METH_CERT_SHA256:
		hashlen = RB_SSL_CERTFP_LEN_SHA256;
		md_type = EVP_sha256();
		item = ASN1_ITEM_rptr(X509);
		data = cert;
		break;
	case RB_SSL_CERTFP_METH_CERT_SHA512:
		hashlen = RB_SSL_CERTFP_LEN_SHA512;
		md_type = EVP_sha512();
		item = ASN1_ITEM_rptr(X509);
		data = cert;
		break;
	case RB_SSL_CERTFP_METH_SPKI_SHA256:
		hashlen = RB_SSL_CERTFP_LEN_SHA256;
		md_type = EVP_sha256();
		item = ASN1_ITEM_rptr(X509_PUBKEY);
		data = X509_get_X509_PUBKEY(cert);
		break;
	case RB_SSL_CERTFP_METH_SPKI_SHA512:
		hashlen = RB_SSL_CERTFP_LEN_SHA512;
		md_type = EVP_sha512();
		item = ASN1_ITEM_rptr(X509_PUBKEY);
		data = X509_get_X509_PUBKEY(cert);
		break;
	default:
		return 0;
	}

	if(ASN1_item_digest(item, md_type, data, certfp, &hashlen) != 1)
	{
		rb_lib_log("%s: ASN1_item_digest: %s", __func__, rb_ssl_strerror(rb_ssl_last_err()));
		return 0;
	}

	return (int) hashlen;
}



/*
 * External OpenSSL-specific code
 */

void
rb_ssl_shutdown(rb_fde_t *const F)
{
	if(F == NULL || F->ssl == NULL)
		return;

	(void) rb_ssl_last_err();

	for(int i = 0; i < 4; i++)
	{
		int ret = SSL_shutdown(SSL_P(F));
		int err = SSL_get_error(SSL_P(F), ret);

		if(ret >= 0 || (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE))
			break;
	}

	SSL_free(SSL_P(F));
	F->ssl = NULL;
}

int
rb_init_ssl(void)
{
#ifndef LRB_SSL_NO_EXPLICIT_INIT
	(void) SSL_library_init();
	SSL_load_error_strings();
#endif

	rb_lib_log("%s: OpenSSL backend initialised", __func__);
	return 1;
}

int
rb_setup_ssl_server(const char *const certfile, const char *keyfile,
                    const char *const dhfile, const char *cipherlist)
{
	if(certfile == NULL)
	{
		rb_lib_log("%s: no certificate file specified", __func__);
		return 0;
	}

	if(keyfile == NULL)
		keyfile = certfile;

	if(cipherlist == NULL)
		cipherlist = rb_default_ciphers;


	(void) rb_ssl_last_err();

	#ifdef LRB_HAVE_TLS_METHOD_API
	SSL_CTX *const ssl_ctx_new = SSL_CTX_new(TLS_method());
	#else
	SSL_CTX *const ssl_ctx_new = SSL_CTX_new(SSLv23_method());
	#endif

	if(ssl_ctx_new == NULL)
	{
		rb_lib_log("%s: SSL_CTX_new: %s", __func__, rb_ssl_strerror(rb_ssl_last_err()));
		return 0;
	}

	if(SSL_CTX_use_certificate_chain_file(ssl_ctx_new, certfile) != 1)
	{
		rb_lib_log("%s: SSL_CTX_use_certificate_chain_file ('%s'): %s", __func__, certfile,
		           rb_ssl_strerror(rb_ssl_last_err()));

		SSL_CTX_free(ssl_ctx_new);
		return 0;
	}

	if(SSL_CTX_use_PrivateKey_file(ssl_ctx_new, keyfile, SSL_FILETYPE_PEM) != 1)
	{
		rb_lib_log("%s: SSL_CTX_use_PrivateKey_file ('%s'): %s", __func__, keyfile,
		           rb_ssl_strerror(rb_ssl_last_err()));

		SSL_CTX_free(ssl_ctx_new);
		return 0;
	}

	if(dhfile == NULL)
	{
		rb_lib_log("%s: no DH parameters file specified", __func__);
	}
	else
	{
		BIO *const dhf = BIO_new_file(dhfile, "r");
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
		EVP_PKEY* dhp = NULL;
#else
		DH *dhp = NULL;
#endif

		if(dhf == NULL)
		{
			rb_lib_log("%s: BIO_new_file ('%s'): %s", __func__, dhfile,
			           rb_ssl_strerror(rb_ssl_last_err()));
		}
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
		else if(PEM_read_bio_Parameters(dhf, &dhp) == NULL)
		{
			rb_lib_log("%s: PEM_read_bio_Parameters ('%s'): %s", __func__, dhfile,
			           rb_ssl_strerror(rb_ssl_last_err()));
#else
		else if(PEM_read_bio_DHparams(dhf, &dhp, NULL, NULL) == NULL)
		{
			rb_lib_log("%s: PEM_read_bio_DHparams ('%s'): %s", __func__, dhfile,
			           rb_ssl_strerror(rb_ssl_last_err()));
#endif
			BIO_free(dhf);
		}
		else
		{
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
			SSL_CTX_set0_tmp_dh_pkey(ssl_ctx_new, dhp);
#else
			SSL_CTX_set_tmp_dh(ssl_ctx_new, dhp);
			DH_free(dhp);
#endif
			BIO_free(dhf);
		}
	}

	int ret_old = SSL_CTX_set_cipher_list(ssl_ctx_new, cipherlist);

	if (ret_old != 1)
	{
		(void) rb_lib_log("%s: no valid old-style ciphersuites found "
		                  "in ssl_cipher_list; will use defaults",
		                  __func__);

		ret_old = SSL_CTX_set_cipher_list(ssl_ctx_new, rb_default_ciphers);
	}

	#ifndef LRB_HAVE_TLS13
	int ret_new = 0;
	#else
	int ret_new = SSL_CTX_set_ciphersuites(ssl_ctx_new, cipherlist);

	if (ret_new != 1)
	{
		(void) rb_lib_log("%s: no valid new-style ciphersuites found "
		                  "in ssl_cipher_list; will use defaults",
		                  __func__);

		ret_new = SSL_CTX_set_ciphersuites(ssl_ctx_new, rb_default_ciphers);
	}
	#endif

	if (ret_old != 1 && ret_new != 1)
	{
		rb_lib_log("%s: could not configure any ciphers", __func__);
		SSL_CTX_free(ssl_ctx_new);
		return 0;
	}


	SSL_CTX_set_session_cache_mode(ssl_ctx_new, SSL_SESS_CACHE_OFF);
	SSL_CTX_set_verify(ssl_ctx_new, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, verify_accept_all_cb);

	#ifdef SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS
	(void) SSL_CTX_clear_options(ssl_ctx_new, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
	#endif

	#ifndef LRB_HAVE_TLS_METHOD_API
	(void) SSL_CTX_set_options(ssl_ctx_new, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
	#endif

	#ifdef SSL_OP_NO_TICKET
	(void) SSL_CTX_set_options(ssl_ctx_new, SSL_OP_NO_TICKET);
	#endif

	#ifdef SSL_OP_CIPHER_SERVER_PREFERENCE
	(void) SSL_CTX_set_options(ssl_ctx_new, SSL_OP_CIPHER_SERVER_PREFERENCE);
	#endif

	#ifdef SSL_OP_SINGLE_DH_USE
	(void) SSL_CTX_set_options(ssl_ctx_new, SSL_OP_SINGLE_DH_USE);
	#endif

	#ifdef SSL_OP_SINGLE_ECDH_USE
	(void) SSL_CTX_set_options(ssl_ctx_new, SSL_OP_SINGLE_ECDH_USE);
	#endif

	#ifdef LRB_HAVE_TLS_ECDH_AUTO
	(void) SSL_CTX_set_ecdh_auto(ssl_ctx_new, 1);
	#endif

	#ifdef LRB_HAVE_TLS_SET_CURVES
	(void) SSL_CTX_set1_curves_list(ssl_ctx_new, rb_default_curves);
	#else
	#  if (OPENSSL_VERSION_NUMBER >= 0x10000000L) && !defined(OPENSSL_NO_ECDH) && defined(NID_secp384r1)
	EC_KEY *const ec_key = EC_KEY_new_by_curve_name(NID_secp384r1);
	if(ec_key != NULL)
	{
		SSL_CTX_set_tmp_ecdh(ssl_ctx_new, ec_key);
		EC_KEY_free(ec_key);
	}
	else
		rb_lib_log("%s: EC_KEY_new_by_curve_name failed; will not enable ECDHE- ciphers", __func__);
	#  else
	     rb_lib_log("%s: OpenSSL built without ECDH support; will not enable ECDHE- ciphers", __func__);
	#  endif
	#endif


	if(ssl_ctx)
		SSL_CTX_free(ssl_ctx);

	ssl_ctx = ssl_ctx_new;


	rb_lib_log("%s: TLS configuration successful", __func__);
	return 1;
}

int
rb_init_prng(const char *const path, prng_seed_t seed_type)
{
	(void) rb_ssl_last_err();

	if(seed_type == RB_PRNG_FILE && RAND_load_file(path, -1) < 0)
		rb_lib_log("%s: RAND_load_file: %s", __func__, rb_ssl_strerror(rb_ssl_last_err()));

	if(RAND_status() != 1)
	{
		rb_lib_log("%s: RAND_status: %s", __func__, rb_ssl_strerror(rb_ssl_last_err()));
		return 0;
	}

	rb_lib_log("%s: PRNG initialised", __func__);
	return 1;
}

int
rb_get_random(void *const buf, const size_t length)
{
	(void) rb_ssl_last_err();

	if(RAND_bytes(buf, (int) length) != 1)
	{
		rb_lib_log("%s: RAND_bytes: %s", __func__, rb_ssl_strerror(rb_ssl_last_err()));
		return 0;
	}

	return 1;
}

const char *
rb_get_ssl_strerror(rb_fde_t *const F)
{
	return rb_ssl_strerror(F->ssl_errno);
}

int
rb_get_ssl_certfp(rb_fde_t *const F, uint8_t certfp[const RB_SSL_CERTFP_LEN], const int method)
{
	if(F == NULL || F->ssl == NULL)
		return 0;

	X509 *const peer_cert = SSL_get_peer_certificate(SSL_P(F));
	if(peer_cert == NULL)
		return 0;

	int len = 0;

	switch(SSL_get_verify_result(SSL_P(F)))
	{
	case X509_V_OK:
	case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
	case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
	case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
	case X509_V_ERR_CERT_UNTRUSTED:
		len = make_certfp(peer_cert, certfp, method);
		// fallthrough
	default:
		X509_free(peer_cert);
		return len;
	}
}

int
rb_get_ssl_certfp_file(const char *const filename, uint8_t certfp[const RB_SSL_CERTFP_LEN], const int method)
{
	FILE *const fp = fopen(filename, "r");
	if (fp == NULL)
		return -1;

	X509 *const cert = PEM_read_X509(fp, NULL, NULL, NULL);
	if (cert == NULL)
	{
		fclose(fp);
		return 0;
	}

	int len = make_certfp(cert, certfp, method);

	X509_free(cert);
	fclose(fp);

	return len;
}

void
rb_get_ssl_info(char *const buf, const size_t len)
{
#ifdef LRB_SSL_FULL_VERSION_INFO
	if(LRB_SSL_VNUM_RUNTIME == LRB_SSL_VNUM_COMPILETIME)
		(void) snprintf(buf, len, "OpenSSL: compiled 0x%lx, library %s",
		                LRB_SSL_VNUM_COMPILETIME, LRB_SSL_VTEXT_COMPILETIME);
	else
		(void) snprintf(buf, len, "OpenSSL: compiled (0x%lx, %s), library (0x%lx, %s)",
		                LRB_SSL_VNUM_COMPILETIME, LRB_SSL_VTEXT_COMPILETIME,
		                LRB_SSL_VNUM_RUNTIME, LRB_SSL_VTEXT_RUNTIME);
#else
	(void) snprintf(buf, len, "OpenSSL: compiled 0x%lx, library %s",
	                LRB_SSL_VNUM_COMPILETIME, LRB_SSL_VTEXT_RUNTIME);
#endif
}

const char *
rb_ssl_get_cipher(rb_fde_t *const F)
{
	if(F == NULL || F->ssl == NULL)
		return NULL;

	static char buf[512];

	const char *const version = SSL_get_version(SSL_P(F));
	const char *const cipher = SSL_get_cipher_name(SSL_P(F));

	(void) snprintf(buf, sizeof buf, "%s, %s", version, cipher);

	return buf;
}

ssize_t
rb_ssl_read(rb_fde_t *const F, void *const buf, const size_t count)
{
	return rb_ssl_read_or_write(0, F, buf, NULL, count);
}

ssize_t
rb_ssl_write(rb_fde_t *const F, const void *const buf, const size_t count)
{
	return rb_ssl_read_or_write(1, F, NULL, buf, count);
}



/*
 * Internal library-agnostic code
 */

static void
rb_ssl_connect_realcb(rb_fde_t *const F, const int status, struct ssl_connect *const sconn)
{
	lrb_assert(F->connect != NULL);

	F->connect->callback = sconn->callback;
	F->connect->data = sconn->data;

	rb_connect_callback(F, status);
	rb_free(sconn);
}

static void
rb_ssl_timeout_cb(rb_fde_t *const F, void *const data __attribute__((unused)))
{
	lrb_assert(F->accept != NULL);
	lrb_assert(F->accept->callback != NULL);

	F->accept->callback(F, RB_ERR_TIMEOUT, NULL, 0, F->accept->data);
}

static void
rb_ssl_tryconn_timeout_cb(rb_fde_t *const F, void *const data)
{
	rb_ssl_connect_realcb(F, RB_ERR_TIMEOUT, data);
}

static void
rb_ssl_tryconn(rb_fde_t *const F, const int status, void *const data)
{
	lrb_assert(F != NULL);

	struct ssl_connect *const sconn = data;

	if(status != RB_OK)
	{
		rb_ssl_connect_realcb(F, status, sconn);
		return;
	}

	F->type |= RB_FD_SSL;

	rb_settimeout(F, sconn->timeout, rb_ssl_tryconn_timeout_cb, sconn);
	rb_ssl_init_fd(F, RB_FD_TLS_DIRECTION_OUT);
	rb_ssl_connect_common(F, sconn);
}



/*
 * External library-agnostic code
 */

int
rb_supports_ssl(void)
{
	return 1;
}

unsigned int
rb_ssl_handshake_count(rb_fde_t *const F)
{
	return F->handshake_count;
}

void
rb_ssl_clear_handshake_count(rb_fde_t *const F)
{
	F->handshake_count = 0;
}

void
rb_ssl_start_accepted(rb_fde_t *const F, ACCB *const cb, void *const data, const int timeout)
{
	F->type |= RB_FD_SSL;

	F->accept = rb_malloc(sizeof(struct acceptdata));
	F->accept->callback = cb;
	F->accept->data = data;
	F->accept->addrlen = 0;
	(void) memset(&F->accept->S, 0x00, sizeof F->accept->S);

	rb_settimeout(F, timeout, rb_ssl_timeout_cb, NULL);
	rb_ssl_init_fd(F, RB_FD_TLS_DIRECTION_IN);
	rb_ssl_accept_common(F, NULL);
}

void
rb_ssl_accept_setup(rb_fde_t *const srv_F, rb_fde_t *const cli_F, struct sockaddr *const st, const int addrlen)
{
	cli_F->type |= RB_FD_SSL;

	cli_F->accept = rb_malloc(sizeof(struct acceptdata));
	cli_F->accept->callback = srv_F->accept->callback;
	cli_F->accept->data = srv_F->accept->data;
	cli_F->accept->addrlen = (rb_socklen_t) addrlen;
	(void) memset(&cli_F->accept->S, 0x00, sizeof cli_F->accept->S);
	(void) memcpy(&cli_F->accept->S, st, (size_t) addrlen);

	rb_settimeout(cli_F, 10, rb_ssl_timeout_cb, NULL);
	rb_ssl_init_fd(cli_F, RB_FD_TLS_DIRECTION_IN);
	rb_ssl_accept_common(cli_F, NULL);
}

int
rb_ssl_listen(rb_fde_t *const F, const int backlog, const int defer_accept)
{
	int result = rb_listen(F, backlog, defer_accept);

	F->type = RB_FD_SOCKET | RB_FD_LISTEN | RB_FD_SSL;

	return result;
}

void
rb_connect_tcp_ssl(rb_fde_t *const F, struct sockaddr *const dest, struct sockaddr *const clocal,
                   CNCB *const callback, void *const data, const int timeout)
{
	if(F == NULL)
		return;

	struct ssl_connect *const sconn = rb_malloc(sizeof *sconn);
	sconn->data = data;
	sconn->callback = callback;
	sconn->timeout = timeout;

	rb_connect_tcp(F, dest, clocal, rb_ssl_tryconn, sconn, timeout);
}

void
rb_ssl_start_connected(rb_fde_t *const F, CNCB *const callback, void *const data, const int timeout)
{
	if(F == NULL)
		return;

	struct ssl_connect *const sconn = rb_malloc(sizeof *sconn);
	sconn->data = data;
	sconn->callback = callback;
	sconn->timeout = timeout;

	F->connect = rb_malloc(sizeof(struct conndata));
	F->connect->callback = callback;
	F->connect->data = data;
	F->type |= RB_FD_SSL;

	rb_settimeout(F, sconn->timeout, rb_ssl_tryconn_timeout_cb, sconn);
	rb_ssl_init_fd(F, RB_FD_TLS_DIRECTION_OUT);
	rb_ssl_connect_common(F, sconn);
}

#endif /* HAVE_OPENSSL */
