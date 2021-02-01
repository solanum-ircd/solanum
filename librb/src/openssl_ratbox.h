/*
 *  libratbox: a library used by ircd-ratbox and other things
 *  openssl_ratbox.h: OpenSSL backend data
 *
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

#ifndef LRB_OPENSSL_H_INC
#define LRB_OPENSSL_H_INC 1

#include <openssl/dh.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

#include <openssl/opensslv.h>

/*
 * A long time ago, in a world far away, OpenSSL had a well-established mechanism for ensuring compatibility with
 * regards to added, changed, and removed functions, by having an SSLEAY_VERSION_NUMBER macro. This was then
 * renamed to OPENSSL_VERSION_NUMBER, but the old macro was kept around for compatibility until OpenSSL version
 * 1.1.0.
 *
 * Then the OpenBSD developers decided that having OpenSSL in their codebase was a bad idea. They forked it to
 * create LibreSSL, gutted all of the functionality they didn't want or need, and generally improved the library
 * a lot. Then, as the OpenBSD developers are want to do, they packaged up LibreSSL for release to other
 * operating systems, as LibreSSL Portable. Think along the lines of OpenSSH where they have also done this.
 *
 * The fun part of this story ends there. LibreSSL has an OPENSSL_VERSION_NUMBER macro, but they have set it to a
 * stupidly high value, version 2.0. OpenSSL version 2.0 does not exist, and LibreSSL 2.2 does not implement
 * everything OpenSSL 1.0.2 or 1.1.0 do. This completely breaks the entire purpose of the macro.
 *
 * The ifdef soup below is for LibreSSL compatibility. Please find whoever thought setting OPENSSL_VERSION_NUMBER
 * to a version that does not exist was a good idea. Encourage them to realise that it is not.   -- amdj
 */

#if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x10100000L)
#  define LRB_SSL_NO_EXPLICIT_INIT      1
#endif

#if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x10002000L)
#  define LRB_HAVE_TLS_SET_CURVES       1
#  if (OPENSSL_VERSION_NUMBER < 0x10100000L)
#    define LRB_HAVE_TLS_ECDH_AUTO      1
#  endif
#endif

#if defined(LIBRESSL_VERSION_NUMBER) && (LIBRESSL_VERSION_NUMBER >= 0x20020002L)
#  define LRB_HAVE_TLS_METHOD_API       1
#else
#  if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x10100000L)
#    define LRB_HAVE_TLS_METHOD_API     1
#  endif
#endif

#if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x10100000L)
#  define LRB_SSL_VTEXT_COMPILETIME     OPENSSL_VERSION_TEXT
#  define LRB_SSL_VTEXT_RUNTIME         OpenSSL_version(OPENSSL_VERSION)
#  define LRB_SSL_VNUM_COMPILETIME      OPENSSL_VERSION_NUMBER
#  define LRB_SSL_VNUM_RUNTIME          OpenSSL_version_num()
#  define LRB_SSL_FULL_VERSION_INFO     1
#else
#  if defined(LIBRESSL_VERSION_NUMBER) && (LIBRESSL_VERSION_NUMBER >= 0x20200000L)
#    define LRB_SSL_VTEXT_RUNTIME       SSLeay_version(SSLEAY_VERSION)
#    define LRB_SSL_VNUM_COMPILETIME    LIBRESSL_VERSION_NUMBER
#  else
#    define LRB_SSL_VTEXT_RUNTIME       SSLeay_version(SSLEAY_VERSION)
#    define LRB_SSL_VNUM_COMPILETIME    SSLEAY_VERSION_NUMBER
#  endif
#endif

#if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER > 0x10101000L)
#  define LRB_HAVE_TLS_ECDH_X25519      1
#else
#  if defined(LIBRESSL_VERSION_NUMBER) && (LIBRESSL_VERSION_NUMBER > 0x2050100fL)
#    define LRB_HAVE_TLS_ECDH_X25519    1
#  endif
#endif

#if !defined(LIBRESSL_VERSION_NUMBER) && (OPENSSL_VERSION_NUMBER >= 0x10101000L)
#  define LRB_HAVE_TLS13                1
#endif



/*
 * Default supported ciphersuites (if the user does not provide any) and
 * curves (OpenSSL 1.0.2+). Hardcoded secp384r1 (NIST P-384) is used on
 * OpenSSL 1.0.0 and 1.0.1 (if available).
 *
 * We prefer AEAD ciphersuites first in order of strength, then SHA2
 * ciphersuites, then remaining suites.
 */

static const char rb_default_ciphers[] = ""
#ifdef LRB_HAVE_TLS13
	"TLS_AES_256_GCM_SHA384:"
	"TLS_CHACHA20_POLY1305_SHA256:"
	"TLS_AES_128_GCM_SHA256:"
#endif
	"aECDSA+kEECDH+CHACHA20:"
	"aRSA+kEECDH+CHACHA20:"
	"aRSA+kEDH+CHACHA20:"
	"aECDSA+kEECDH+AESGCM:"
	"aRSA+kEECDH+AESGCM:"
	"aRSA+kEDH+AESGCM:"
	"aECDSA+kEECDH+AESCCM:"
	"aRSA+kEECDH+AESCCM:"
	"aRSA+kEDH+AESCCM:"
	"@STRENGTH:"
	"aECDSA+kEECDH+HIGH+SHA384:"
	"aRSA+kEECDH+HIGH+SHA384:"
	"aRSA+kEDH+HIGH+SHA384:"
	"aECDSA+kEECDH+HIGH+SHA256:"
	"aRSA+kEECDH+HIGH+SHA256:"
	"aRSA+kEDH+HIGH+SHA256:"
	"aECDSA+kEECDH+HIGH:"
	"aRSA+kEECDH+HIGH:"
	"aRSA+kEDH+HIGH:"
	"HIGH:"
	"!3DES:"
	"!aNULL";

#ifdef LRB_HAVE_TLS_SET_CURVES
#  ifdef LRB_HAVE_TLS_ECDH_X25519
static char rb_default_curves[] = "X25519:P-521:P-384:P-256";
#  else
static char rb_default_curves[] = "P-521:P-384:P-256";
#  endif
#endif

#endif /* LRB_OPENSSL_H_INC */
