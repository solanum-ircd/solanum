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

 * **FreeBSD**: if you are compiling with ipv6 you may experience
   problems with ipv4 due to the way the socket code is written.  To
   fix this you must: `sysctl net.inet6.ip6.v6only=0`

 * **Solaris**: you may have to set your `PATH` to include `/usr/gnu/bin` and `/usr/gnu/sbin` before `/usr/bin`
   and `/usr/sbin`. When running as a 32-bit binary, it should be started as:

   ```bash
   ulimit -n 4095 ; LD_PRELOAD_32=/usr/lib/extendedFILE.so.1 ./solanum
   ```

# building

```bash
sudo apt install build-essential pkg-config meson ninja-build libsqlite3-dev flex bison # or equivalent for your distribution
meson setup build --prefix=/path/to/installation
meson compile -C build/
meson test -C build/
meson install -C build/
```

See `meson configure build/` for build options.

# feature specific requirements

 * For SSL/TLS client and server connections, one of:

   * OpenSSL 1.1.0 or newer (`-Dopenssl=enabled`)
   * LibreSSL (`-Dopenssl=enabled`)
   * mbedTLS (`-Dmbedtls=enabled`)
   * GnuTLS (`-Dgnutls=enabled`)

   If multiple TLS libraries are available, the build system will prefer mbedTLS > OpenSSL > GnuTLS.
   Use `=enabled` instead of `=auto` to force a specific library.

 * For certificate-based oper CHALLENGE, OpenSSL 1.1.0 or newer.
   (Using CHALLENGE is not recommended for new deployments, so if you want to use a different TLS library,
    feel free.)

 * For ECDHE under OpenSSL, on Solaris you will need to compile your own OpenSSL on these systems, as they
   have removed support for ECC/ECDHE.  Alternatively, consider using another library (see above).

# tips

 * To report bugs in Solanum, visit us at `#solanum` on [Libera Chat](https://libera.chat)

 * Please read [doc/readme.txt](doc/readme.txt) to get an overview of the current documentation.

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
