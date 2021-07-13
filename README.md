# solanum ![Build Status](https://github.com/solanum-ircd/solanum/workflows/CI/badge.svg)

Solanum is an IRCv3 server designed to be highly scalable.  It implements IRCv3.1 and some parts of IRCv3.2.

It is meant to be used with an IRCv3-capable services implementation such as [Atheme][atheme] or [Anope][anope].

   [atheme]: https://atheme.github.io/
   [anope]: http://www.anope.org/

# necessary requirements

 * A supported platform
 * A working dynamic library system
 * A working lex and yacc - flex and bison should work

# platforms

Solanum is developed on Linux with glibc, but is currently portable to most POSIX-compatible operating systems.
However, this portability is likely to be removed unless someone is willing to maintain it.  If you'd like to be that
person, please let us know on IRC.

# platform specific errata

These are known issues and workarounds for various platforms.

 * **macOS**: you must set the `LIBTOOLIZE` environment variable to point to glibtoolize before running autogen.sh:

   ```bash
   brew install libtool
   export LIBTOOLIZE="/usr/local/bin/glibtoolize"
   ./autogen.sh
   ```

 * **FreeBSD**: if you are compiling with ipv6 you may experience
   problems with ipv4 due to the way the socket code is written.  To
   fix this you must: `sysctl net.inet6.ip6.v6only=0`

 * **Solaris**: you may have to set your `PATH` to include `/usr/gnu/bin` and `/usr/gnu/sbin` before `/usr/bin`
   and `/usr/sbin`. Solaris's default tools don't seem to play nicely with the configure script. When running
   as a 32-bit binary, it should be started as:

   ```bash
   ulimit -n 4095 ; LD_PRELOAD_32=/usr/lib/extendedFILE.so.1 ./solanum
   ```

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

   * OpenSSL 1.0.0 or newer (`--enable-openssl`)
   * LibreSSL (`--enable-openssl`)
   * mbedTLS (`--enable-mbedtls`)
   * GnuTLS (`--enable-gnutls`)

 * For certificate-based oper CHALLENGE, OpenSSL 1.0.0 or newer.
   (Using CHALLENGE is not recommended for new deployments, so if you want to use a different TLS library,
    feel free.)

 * For ECDHE under OpenSSL, on Solaris you will need to compile your own OpenSSL on these systems, as they
   have removed support for ECC/ECDHE.  Alternatively, consider using another library (see above).

# tips

 * To report bugs in Solanum, visit us at `#solanum` on [Libera Chat](https://libera.chat)

 * Please read [doc/index.txt](doc/index.txt) to get an overview of the current documentation.

 * Read the [NEWS.md](NEWS.md) file for what's new in this release.

 * The files, `/etc/services`, `/etc/protocols`, and `/etc/resolv.conf`, SHOULD be
   readable by the user running the server in order for ircd to start with
   the correct settings.  If these files are wrong, Solanum will try to use
   `127.0.0.1` for a resolver as a last-ditch effort.

# git access

 * The Solanum git repository can be checked out using the following command:
	`git clone https://github.com/solanum-ircd/solanum`

 * Solanum's git repository can be browsed over the Internet at the following address:
	https://github.com/solanum-ircd/solanum
