# solanum [![Build Status](https://travis-ci.com/solanum-ircd/solanum.svg?branch=main)](https://travis-ci.com/solanum-ircd/solanum)

Solanum is an IRCv3 server designed to be highly scalable.  It implements IRCv3.1 and some parts of IRCv3.2.

It is meant to be used with an IRCv3-capable services implementation such as [Atheme][atheme] or [Anope][anope].

   [atheme]: http://www.atheme.net/
   [anope]: http://www.anope.org/

# necessary requirements

 * A supported platform
 * A working dynamic library system
 * A working lex and yacc - flex and bison should work

# platforms

Solanum is designed with portability in mind, but does not target older systems nor those of solely academic
interest.

Do note that operating systems are only supported if they are supported by their vendor.

## Tier 1

These platforms are the best supported, and should always work. They are actively tested. If you encounter
problems, please file a bug.

* FreeBSD 10.x and above (i386 and amd64)
* Linux 2.6.x and above with glibc or musl (i386, x86_64, and ARM)
* macOS 10.7 and above
* Windows Vista/Server 2008 and above (x86 or x64)

## Tier 2

These platforms are supported and occasionally tested, and most features should work, but this is not
guaranteed. If you find any problems, file a bug, but as these are not regularly tested platforms, a timely
resolution may not be possible.

* DragonflyBSD 4.4 and above (i386)
* Linux with uClibc (i386 or x86_64)
* NetBSD 6.1.x and above (i386, amd64)
* OpenBSD 5.6 and above (i386, amd64)
* Solaris 10 and above (i386)

## Tier 3

Anything else that hasn't been tested. Solanum may or may not work on it; patches welcome if they don't.

# platform specific errata

These are known issues and workarounds for supported platforms.

 * **macOS**: you must set the `LIBTOOLIZE` environment variable to point to glibtoolize before running autogen.sh:

   ```bash
   brew install libtool
   export LIBTOOLIZE="/usr/local/bin/glibtoolize"
   ./autogen.sh
   ```

 * **FreeBSD**: if you are compiling with ipv6 you may experience
   problems with ipv4 due to the way the socket code is written.  To
   fix this you must: "sysctl net.inet6.ip6.v6only=0"

 * **Solaris**: you may have to set your PATH to include /usr/gnu/bin and /usr/gnu/sbin before /usr/bin
   and /usr/sbin. Solaris's default tools don't seem to play nicely with the configure script.

# building

```bash
./autogen.sh
./configure --prefix=/path/to/installation
make
make check # run tests
make install
```

See `./configure --help` for build options.

# feature specific requirements

 * For SSL/TLS client and server connections, one of:

   * OpenSSL 1.0.0 or newer (--enable-openssl)
   * LibreSSL (--enable-openssl)
   * mbedTLS (--enable-mbedtls)
   * GnuTLS (--enable-gnutls)

 * For certificate-based oper CHALLENGE, OpenSSL 1.0.0 or newer.
   (Using CHALLENGE is not recommended for new deployments, so if you want to use a different TLS library,
    feel free.)

 * For ECDHE under OpenSSL, on Solaris and RHEL/Fedora (and its derivatives such as CentOS) you will
   need to compile your own OpenSSL on these systems, as they have removed support for ECC/ECDHE.
   Alternatively, consider using another library (see above).

# tips

 * To report bugs in solanum, visit us at chat.freenode.net #solanum

 * Please read doc/index.txt to get an overview of the current documentation.

 * Read the NEWS file for what's new in this release.

 * The files, /etc/services, /etc/protocols, and /etc/resolv.conf, SHOULD be
   readable by the user running the server in order for ircd to start with
   the correct settings.  If these files are wrong, solanum will try to use
   127.0.0.1 for a resolver as a last-ditch effort.

# git access

 * The Solanum GIT repository can be checked out using the following command:
	`git clone https://github.com/solanum-ircd/solanum`

 * Solanum's GIT repository depot can be browsed over the Internet at the following address:
	https://github.com/solanum-ircd/solanum
