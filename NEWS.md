# News

This is solanum 1.0-dev.
See LICENSE for licensing details (GPL v2).

## solanum-1.0

Includes changes from charybdis-4.1.3-dev.

**This release includes breaking changes from charybdis 4.x.** Please pay close attention to
bolded warnings in the full release notes below.

### build
- Add `--with-asan` to configure to produce an ASan instrumented build

### server protocol
- **Breaking:** Don't implicitly abort SASL when connection registration handshake completes;
  requires updating atheme to include https://github.com/atheme/atheme/pull/833.
- OPER is now propagated globally, as :operator OPER opername privset

### user
- **Breaking:** invite-notify is now enabled by loading the invite-notify extension
- Prioritise older, more important client capabilities for clients that can only accept
  one line of CAP LS
- Add the solanum.chat/realhost vendor capability (provided by extensions/cap\_realhost)
- Add the solanum.chat/identify-msg vendor capability (provided by extensions/identify\_msg)
- Server-side aliases preserve protocol framing characters
- Add the +G user mode for soft callerid (implicitly allow users with a common channel)
- /invite no longer punches through callerid
- invite-notify now works
- Rejectcached users are now sent the reason of the ban that caused their reject in most cases
- Rejectcache entries expire when their corresponding K-lines do
- One-argument /stats and zero-argument /motd are no longer ratelimited
- Channel bans don't see through IP spoofs
- Global /names now respects userhost-in-names
- The `$j` extban is no longer usable inside ban exceptions
- TLSv1 connections are accepted. They can still be disabled using OpenSSL config if you don't
  want them. TLSv1 existing is not thought to be a threat to up-to-date clients.

### oper
- **Breaking:** Kick immunity for override is now its own extension, override\_kick\_immunity
- **Breaking:** /stats A output now follows the same format as other stats letters
- **Breaking:** helpops now uses +h instead of +H
- **Breaking:** sno\_whois and the spy\_ extensions have been removed
- **Breaking:** Using /wallops now requires the oper:wallops privilege instead of oper:massnotice
- Opers now have their privset (identified by name) on remote servers
- Oper-only umodes are refreshed after rehash and /grant
- Extension modules can be reloaded
- Override no longer spams about being enabled/disabled. It continues to spam on each use.
- Add /testkline, which has the same syntax as /testline but doesn't check if the mask is ilined
- /privs is now remote-capable and can respond with more than one line
- Most commands now respect oper hiding
- Massnotice (notice/privmsg to $$.../$#...) now alerts opers
- Massnotice no longer imposes any restrictions on the target mask
- /kline and /dline are hardened to invalid inputs
- K/D-lines are more consistent about checking for encoded ipv4-in-ipv6 addresses
- Add extensions/drain to reject new connections
- Add extensions/filter to filter messages, parts and quits with a Hyperscan database
- Add extensions/sasl\_usercloak to interpolate SASL account names into I-line spoofs

### conf
- **Breaking:** Completely overhaul oper privs. All privset configs will need to be rewritten.
  See reference.conf for details.
- Add the `kline_spoof_ip` I-line flag to make any spoof opaque to K-line matching
- Add general::hide\_tkline\_duration to remove durations from user-visible ban reasons
- Add general::hide\_opers, which behaves as if all opers have oper:hidden
- Add general::post\_registration\_delay
- Add general::tls\_ciphers\_oper\_only to hide TLS cipher details in /whois
- Add channel::opmod\_send\_statusmsg to send messages allowed by +z to @#channel
- Add class::max\_autoconn, with the behaviour of class::max\_number for servers prior to
  charybdis 4
- Add `secure {}` blocks. Networks listed in a secure block gain +Z and can match `need_ssl` I-
  and O-lines.
- Remove general::kline\_delay
- If m\_webirc is loaded, connections that try to use a webirc auth block as their I-line will
  be disconnected on registration

### misc
- **Breaking:** WebSocket support has been removed.
- **Breaking:** WEBIRC now processes the "secure" option as specified by IRCv3. Web gateways that
  do not set this option will need to be updated or their connections will show as insecure.
- Successfully changing IP with WEBIRC now drops an identd username

### code
- Channel lists are now kept sorted. A for-loop macro, `ITER_COMM_CHANNELS`, is introduced to
  efficiently compare two such lists.


## charybdis-4.1.2

### user
- src/s\_user.c: don't corrupt usermodes on module unload/reload

## charybdis-4.1.1

### security
- Fix an issue with the PASS command and duplicate server instances.

### misc
- Fix connection hang with blacklist/opm when ident is disabled.
- Improve SASL CAP notification when the services server disconnects.
- MbedTLS: Support ChaCha20-Poly1305 in default cipher suites.

## charybdis-4.1

### misc
- SCTP is now supported for server connections (and optionally, user connections)

## charybdis-4.0.1

### server protocol
- SJOIN messages were being constructed in a 1024 byte buffer and truncated to 512 bytes
  when sending. This caused channels with more than 50 users to fail to propagate all of
  them during a net join.

## charybdis-4.0

### build
- Build system has been converted to libtool + automake for sanity reasons.
- The compile date is now set at configure time rather than build time, allowing for
  reproducible builds. (#148, #149)
- Support for GNUTLS 3.4 has been added.

### user
- Import the ability to exceed MAXCHANNELS from ircd-seven.
- Implement IRCv3.2 enhanced capability negotiation (`CAP LS 302`).
- Implement support for receiving and sending IRCv3 message tags.
- Implement IRCv3.2 capabilities: (#141)
  - account-tag
  - echo-message
  - invite-notify
  - sasl
  - server-time
- SASL: certificate fingerprints are now always sent to the SASL agent, allowing for
  the certificate to be used as a second authentication factor.

### oper
- Merge several features from ircd-seven:
  - Implement support for remote DIE/RESTART.
  - Implement support for remote MODLOAD et al commands.
  - Add the GRANT command which allows for temporarily opering a client.
  - Implement the hidden oper-only channel modes framework.
  - Implement a channel mode that disallows kicking IRC operators (+M).
- Enhance the oper override system, allowing more flexibility and detail
  in network-wide notices.
- DNS, ident, and blacklist lookups have been moved to a dedicated daemon known
  as authd. Some cosmetic changes to blacklist statistics and rejection notices
  have resulted.
- An experimental OPM scanner has been added to authd. Plaintext SOCKS4,
  SOCKS5, and HTTP CONNECT proxies can be checked for.
- The LOCOPS command has been moved from core to an extension.
- All core modules in charybdis have descriptions, which are shown in MODLIST.
- Suffixes should not be used when doing /MODLOAD, /MODUNLOAD, /MODRELOAD, etc.

### misc
- Support for WebSocket has been added, use the listen::wsock option to switch
  a listener into websocket mode.

### conf
- Add the ability to strip color codes from topics unconditionally.
- The obsolete hub option from server info has been removed.

### docs
- The documentation has been cleaned up; obsolete files have been purged, and
  files have been renamed and shuffled around to be more consistent.

### code
- `common.h` is gone. Everything useful in it was moved to `ircd_defs.h`.
- `config.h` is gone; the few remaining knobs in it were not for configuration
  by mere mortals, and mostly existed as a 2.8 relic. Most of the knobs live in
  `defaults.h`, but one is well-advised to stay away unless they know exactly
  what they are doing.
- A new module API has been introduced, known as AV2. It includes things such as
  module datecodes (to ensure modules don't fall out of sync with the code),
  module descriptions, and other fun things.
- Alias and module commands are now in m_alias and m_modules, respectively, and
  can be reloaded if need be. For sanity reasons, m_modules is a core module,
  and cannot be unloaded.
- irc_dictionary and irc_radixtree related functions are now in librb, and
  prefixed accordingly. Typedefs have been added for consistency with existing
  data structures. For example, now you would write `rb_dictionary *foo` and
  `RB_DICTIONARY_FOREACH`.
- C99 bools are now included and used in the code. Don't use ints as simple true
  or false flags anymore. In accordance with this change, the `YES`/`NO` and
  `TRUE`/`FALSE` macros have been removed.
- Return types from command handlers have been axed, as they have been useless
  for years.
- libratbox has been renamed to librb, as we have diverged from upstream long
  ago.
- Almost all 2.8-style hashtable structures have been moved to dictionaries or
  radix trees, resulting in significant memory savings.
- The block allocator has been disabled and is no longer used.
- The ratbox client capabilities have been ported to use the ircd capabilities
  framework, allowing for modules to provide capabilities.
- Support for restarting ssld has been added.  ssld processes which are still
  servicing clients will remain in use, but not service new connections, and
  are garbage collected when they are no longer servicing connections.
- Support for ratbox-style 'iodebug' hooks has been removed.
- New channel types may be added by modules, see `extensions/chantype_dummy.c`
  for a very simple example.

## charybdis-3.5.0

### server protocol
- Fix propagation of ip_cloaking hostname changes (only when setting or
  unsetting the umode after connection).
- Fix a remote-triggerable crash triggered by the CAPAB parsing code.
- As per the TS6 spec, require QS and ENCAP capabilities.
- Require EX and IE capabilities (+e and +I cmodes).
- Check that UIDs start with the server's SID.

### user
- Allow mode queries on mlocked modes. In particular, allow /mode #channel f
  to query the forward channel even if +f is mlocked.
- Strip colours from channel topics in /list.
- If umode +D or +g are oper-only, don't advertise them in 005.
- If MONITOR is not enabled, don't advertise it in 005.
- Add starttls as per ircv3.
- Abort a whowas listing when it would exceed SendQ, which would previously
  disconnect the user.
- Reject nicks with '~' in them, rather than truncating at the '~'.
- Remove CHARSET=ascii from ISUPPORT
- Use the normal rules for IP visibility in /whowas.
- Cmode +c now strips '\x0F' (^O, formatting off), fixing weird rendering in
  some clients that internally use mIRC formatting such as highlighted
  messages in HexChat.
- Indicate join failure because of the chm_sslonly extension (cmode +S) using
  the same 480 numeric as ircd-ratbox.
- Do not allow SASL authentication when the configured SASL agent is unavailable.
- Automatically add unidentified users to the ACCEPT list when a user is set +R,
  as we do when the user is set +g.
- Implement IRCv3.2 capabilities:
  - cap-notify
  - chghost
  - userhost-in-names
- Implement the $&, $| and $m extban types:
  - $& combines 1 or more child extbans as an AND expression
  - $| combines 1 or more child extbans as an OR expression
  - $m provides normal hostmask matching as an extban for the above
- Do not allow STARTTLS if a connection is already using TLS.
- Display an operator's privilege set in WHOIS.
- The $o extban now matches against privilege set names as well as individual
  privileges.  Privilege set names are preferred over individual privileges.

### oper
- Fix a crash with /testline.
- Complain to opers if a server that isn't a service tries to
  SU/RSFNC/NICKDELAY/SVSLOGIN.
- Turn off umode +p (override) when deopering.
- Make listener error messages (e.g. port already in use) visible by default
  instead of only on snomask +d and in ioerrorlog.
- Remove snotes on +r about GET/PUT/POST commands ("HTTP Proxy disconnected").
- Add DNSBL snotes on snomask +r.

### config
- Add hide_uncommon_channels extension to hide uncommon channel memberships in WHOIS,
  like in ircd-seven.
- Add chm_nonotice extension, cmode +T to reject notices.
- Add restrict-unauthenticated extension, prevents unauthenticated users from
  doing anything as channel operator.
- Add no_kill_services extension, prevents local opers from killing services.
- Allow matching specific replies of DNSBLs, using the new matches option.
- Remove blowfish crypt since it has the BSD advertising clause.
- Fix SHA256 ($5$) crypt.
- Make the channel::channel_target_change option actually work (it used to be
  always on).
- SSL/TLS listeners now have defer_accept unconditionally enabled on them.
- The method used for certificate fingerprints (CertFP) is now configurable.
  SHA1, SHA256 and SHA512 are available options.
- The minimum user threshold for channels in default /list output is now
  configurable.

### misc
- Work around timerfd/signalfd brokenness on OpenVZ.
- Fix a compilation issue in libratbox/src/sigio.c with recent glibc.
- Extend documentation slightly.
- Remove a BSD advertising clause that permission was granted to remove.
- Add support for hooking PRIVMSG/NOTICE.
- Reenable and fix the GnuTLS support.
- Add mbedTLS backend for SSL/TLS.
- Remove EGD support.
- Try other DNS servers if errors or corrupt replies are encountered.
- Rename genssl.sh script to genssl.
- Choose more secure SSL/TLS algorithms.
- Fix reconnecting with SSL/TLS with some clients such as ChatZilla (see
  https://bugzilla.mozilla.org/show_bug.cgi?id=858394#c34 for details.)
- Improve error messages about the configuration file.
- Fix a crash when compiled with recent clang on 32-bit systems.
- Fix various memory leaks in rehash.
- Fix various code quality issues.
- Add --with-shared-sqlite to allow distribution packages to link to a shared
  sqlite library. Using this is not recommended for on-server compilation.
- ISUPPORT tokens which are actually provided by modules have been moved to their
  respective modules.

## charybdis-3.4.0

### server protocol
- Allow overriding opers (with the new extension) to op themselves on channels.
- Allow RSFNC to change a nickname's capitalization only.
- Add channel ban forwarding <mask>$<channel> much like ircd-seven. Local use
  of this is controlled by the channel::use_forward config option.
- Add ENCAP TGINFO to propagate IP addresses that exceeded target change
  limits (these get a lower limit when they reconnect).

### user
- Consider bogus CTCP ACTION messages (without action text) CTCP (for
  cmode +C).
- Send ERR_TOOMANYCHANNELS for each channel join that fails due to channel
  limits.
- Add account-notify client capability to notify clients about logins and
  logouts of users in common channels. See doc/account-notify.txt.
- Add extended-join client capability to add account name and ircname to JOIN.
- Add topic TS and channel TS constraints for /LIST (T<, T>, C<, C>
  parameters as in some other servers).
- Disallow wildcarded nicknames in "hunted" parameters like /stats and /motd.
- Disallow mIRC italics in channel names when disable_fake_channels.
- Add AUTHENTICATE EXTERNAL support, allows SASL authentication using a
  certificate fingerprint.
- Allow channel::kick_on_split_riding to protect channels with mlocked keys.
- The NICKLEN token in 005 now only specifies the maximum usable nick length.
  The MAXNICKLEN token specifies the maximum nick length any user can have.
- Disallow $ in usernames as this may cause problems with ban forwarding.
- Add an error message (numeric 743) if a ban mask is invalid.
- Extract the underlying IPv4 address from 6to4 and Teredo IPv6 addresses.
  Show it in a remote /whois and check channel bans, quiets, D:lines and
  K:lines against it. Note that ban exceptions and auth{} blocks are not
  checked.
- Allow normal users to perform /privs on themselves, showing some privileges
  from the auth{} block.
- Add away-notify client capability, see doc/away-notify.txt.
- Add rate limit for high-bandwidth commands, in particular /who <channel>.
- Rate limit /away to help avoid flooding via away-notify.
- Apply colour stripping (cmode +c) and CTCP checking (cmode +C) to messages
  to @/+ channel as well.
- Channel mode +c (and other places that disallow colour codes) now also strip
  ASCII 4 (a different kind of colour code).

### oper
- Add operspy for /list.
- Add a server notice to snomask +b if a user exceeds target change limits.
- Add missing server notice for kills from RSFNC and SVSLOGIN.
- Add /stats C to show information about dynamically loaded server
  capabilities.

### config
- Add support for linking using SSL certificate fingerprints as the link
  credential rather than the traditional password pair.
- Add m_roleplay extension, provides various roleplay commands.
- Add override extension, umode +p oper override for opers with oper:override
  permission, with accountability notices and timeout. Note that opers cannot
  op themselves if there are older servers on the network.
- Add channel::disable_local_channels config option.
- Add support for IPv6 DNSBLs. A new "type" option specifies the IP version(s)
  for which each DNSBL should be checked.
- Make flood control settings configurable by those who know exactly what they
  are doing.
- Add serverinfo::nicklen config option to limit the nick length for local
  users. Different values of this option do not break the server protocol.
- Add extb_usermode extension, $m:+-<modes> extban matching against umodes.
- Extend extb_oper extension to allow matching against oper privileges.
- Add m_remove extension, /remove command as in ircd-seven.
- Add general::away_interval to allow configuring /away rate limiting.
- Add listener::defer_accept to delay accepting a connection until the client
  sends data. This depends on kernel support. It may break BOPM checking.

### misc
- In mkpasswd, default to SHA512-based crypt instead of MD5-based crypt.
- Add --with-custom-branding and --with-custom-version configure options to
  help forks/patchsets distinguish themselves.
- Change version control from Mercurial to GIT.
- Ensure SIGHUP and SIGINT keep working after a SIGINT restart.
- Add --enable-fhs-paths configure option to allow installing into a more
  FHS-like hierarchy.
- Remove broken GnuTLS support. SSL/TLS is now only provided using OpenSSL.

## charybdis-3.3.0

### server protocol
- Add new BAN command, for propagated network-wide bans (K/X:lines and RESVs).
  These will burst to new servers as they are introduced, and will stay in sync
  across the whole network (new BAN capab).
- Add new MLOCK command, to implement ircd-side channel mode locks. This allows
  services to send out a list of mode letters for a given channel which may not
  be changed, preventing mode fights between services and client bots (new MLOCK
  capab).

### user
- New RPL_QUIETLIST(728) and RPL_ENDOFQUIETLIST(729) numerics are used for the
  quiet (+q) list, instead of overloading the ban list numerics.
- Users may no longer change the topic of a -t channel if they cannot send to
  it.
- Add help for EXTBAN, describing the syntax of extended bans in general, as
  well as the most common types.
- Changed AWAY messages are now propagated to other servers. Previously, AWAY
  was only propagated when the user was not already away.
- Channel mode +c (and other places that disallow colour codes) now also strip
  ASCII 29 (mIRC 7 italics).
- Add auto-accept for user mode +g (callerid): Messaging a user while set +g
  will automatically add them to your accept list.
- Add target change for channels.  It applies to unopped, unvoiced and unopered
  users.  This has the effect of stopping spambots which join, message and part
  many channels at a time.
- Show RPL_WHOISLOGGEDIN in /whowas as well as in /whois entries.  This adds at
  most an additional 0.5MB of memory usage.
### config
- Add general::use_propagated_bans to switch the new BAN system on or off.
- Add general::default_ident_timeout, to control the timeout for identd (auth)
  connections.
- Add channel::channel_target_change to switch the new channel target change limits
  on or off.
- Fix class::number_per_ident so that it also applies to connections without
  identd.
- Change the example sslport option to 6697, which is more standard than 9999.
### misc
- The custom channel mode API has been rewritten, allowing these modules to work
  correctly when reloaded, or loaded from the config file.
- The EFNet RBL is now recommended, instead of DroneBL.
- Remove the unsupported modules directory.
- Numerous bug fixes and code cleanups.
- In mkpasswd, default to MD5 crypt instead of insecure DES.

## charybdis-3.2.0

### server protocol
- Apply +z to messages blocked by +b and +q as well. (new EOPMOD capab)
- Add new topic command ETB, allowing services to set topic+setter+ts always.
  (new EOPMOD capab)
- The slash ('/') character is now allowed in spoofs.

### user
- Add can_kick hook, based on the ircd-seven one.
- Add cmode +C (no CTCP) from ircd-seven.
- Flood checking has been reworked.
- Fix op-moderate (cmode +z) for channel names with '@'.
- Add CERTFP support, allowing users to connect with an SSL client
  certificate and propagating the certificate fingerprint to other servers.
  Services packages can use this to identify users based on client
  certificates.
- Maintain the list of recently used targets (for the target change
  anti-spam system) in most-recently-used order, overwriting the least
  recently used target with a new one. This should be friendlier to users
  without giving spambots anything.
- Do not require target change slots for replying to the last five users to
  send a private message, notice or invite.
- Apply target change restrictions to /invite.
- Apply umode +g/+R restrictions to /invite, with the difference that
  instead of sending "<user> is messaging you" the invite is let through
  since that is just as noisy.

### oper
- Add /rehash throttles to clear throttling.
- Send all server notices resulting from a remote /rehash to the oper.
- '\s' for space is now part of the matching, not a substitution at xline
  time, fixing various issues with it.
- Display o:line "nickname" in oper-up server notices.
- Fix sendq exceeded snotes for servers.
- SCAN UMODES: default list-max to 500, like a global WHO.
- Ignore directory names in MODRELOAD to avoid crashing if it is a core
  module and the path is incorrect.
- Tweaks to spambot checks.

### config
- Add channel::only_ascii_channels config option to restrict channel names
  to printable ascii only.
- Add channel::resv_forcepart, forcibly parts local users on channel RESV,
  default enabled.

### misc
- New mkpasswd from ircd-ratbox.
- Check more system calls for errors and handle the errors.
- Various ssld/libratbox bugfixes from ircd-ratbox. [some MERGED]
- Fix fd passing on FreeBSD/amd64 and possibly Solaris/sparc. [MERGED]
- Various documentation improvements. [some MERGED]
- Fix some crash issues. [MERGED]
- Add bandb from ircd-ratbox, which stores permanent dlines/klines/xlines/resvs
  in an sqlite database instead of a flatfile and does the storage in a
  helper process. Use bin/bantool -i to import your old bans into the
  database.

## charybdis-3.1.0

- Remove TS5 support. No TS5 servers are permitted in a network with
  charybdis 3.1.0 or newer, except jupes.
- Replace oper flags by privilege sets (privsets). This adds an extra
  level of indirection between oper flags and operator blocks. /stats O
  (capital O) shows the configured privsets.
- Update libratbox and ssld from upstream and use it better.
- Add auth_user to auth{}. This allows specifying a username:password instead
  of just a password in PASS, so that a fixed user@host is not necessary
  for a specific auth{} block.
- Add need_ssl to auth{} and operator{}. This makes these blocks reject
  the user if not connected via SSL.
- Allow modules to provide simple channel modes without parameter.
- Remove restrictions on CNAME in the resolver.
- Make the resolver remember nonresponsive nameservers.
- Move nick collision notices from +s to +k.
- Add additional information to various server notices about server
  connections.
- Show throttle information in /stats t.
- Show rejectcache and throttle information in /testline.
- Show oper reason in /testline.
- Allow opers to see other users' umodes with /mode <nick>.
- SCAN UMODES GLOBAL NO-LIST MASK <mask> is no longer an operspy command.
- Also apply floodcount to messages to remote clients (except services).
- Remove user@server messages to local users. Sending such messages to
  remote servers is still possible, for securely messaging pseudoservers
  whether service{}'ed or not. The special oper-only syntax opers@server
  remains as well.
- Allow /list on a named +p channel. A full /list already included +p channels.
- Add operspy /topic.
- For remote rehashes, send error messages to the requesting oper as well.
- Disable autoconnect for a server with excessive TS delta.
- Disallow invites to juped channels.
- Warn about certain duplicate and redundant auth blocks.
- Make PRIVMSG/NOTICE behave as CPRIVMSG/CNOTICE automatically if possible.
- Allow +z messages from outside if a channel is -n.
- Allow coloured part reasons in -c channels.
- Add ircu-like WHOX support. This allows requesting specific information
  in /who and allows obtaining services login name for all users in a
  channel. XChat/Conspire use WHOX to update away status more efficiently.
- Allow opers and shide_exempt users to see hopcounts even if flatten_links
  is on.
- Rework ip_cloaking.
- Add the IP address to userlog, as in ircd-ratbox 3.0.
- Split cidr_bitlen into cidr_ipv4_bitlen and cidr_ipv6_bitlen.
- Allow using ziplinks with SSL connections. This is not as efficient as
  using OpenSSL's built in compression, but also works with older versions
  of OpenSSL.
- Fix an off by one error with zipstats processing, which could overwrite
  a variable with NULL causing a crash on some systems.
- Document some extensions in charybdis-oper-guide.
- Add more server protocol documentation.
- Add m_sendbans extension, SENDBANS command to propagate xlines and resvs
  manually.
- Add chm_sslonly extension, cmode +S for SSL/TLS only channels.
- Add chm_operonly extension, cmode +O for IRCop only channels.
- Add chm_adminonly extension, cmode +A for server admin only channels.
- Various code cleanups.

## charybdis-3.0.4

- Fix a crash on certain recent versions of Ubuntu.
- Allow 127.x.y.z for DNSBL replies instead of just 127.0.0.x.
- Various documentation improvements.

## charybdis-3.0.3

- Fix IPv6 D:lines
- Fix rejectcache and unknown_count.
- Fix genssl.sh.
- Fix ident for SSL/TLS connections.
- Fix SSL/TLS bugs for servers with more than about 100 connections.
- Small bugfixes.

## charybdis-3.0.2

- Improve OLIST extension error messages.
- Improve some kline error checking.
- Avoid timing out clients if we are still waiting for a DNSBL lookup.
- Fix resolver hangs with epoll.
- Fix compilation without zlib.

## charybdis-3.0.1

- Fix occasional hung clients with kqueue.
- Fix a rare ssld crash.
- Fix a bug that could cause incorrect connect failure reasons to be
  reported.
- Make the IRCd work on MacOS X again.

## charybdis-3.0.0

- Port the IRCd to libratbox, which has improved our portability and allows
  us to reuse low-level code instead of maintaining our own.
- Change configuration of maximum number of clients to ircd-ratbox 3 way.
- Add adminwall from ircd-ratbox, as an extension.
- Add client and server-to-server SSL, read example.conf for setup.
- Replace servlink with ssld (also for ziplinks).
- A new extban, $z, has been added for ssl users (extensions/extb_ssl.so).
- A new compatibility channel mode, +R, has been added, it sets
  +q/-q $~a (extensions/chm_operonly_compat.so). This is similar to
  the +R seen in ircd-seven.
- A new compatibility channel mode, +S, has been added, it sets
  +b/-b $~z (extensions/chm_sslonly_compat.so).
- A new compatibility channel mode, +O, has been added, it sets
  +iI/-iI $o (extensions/chm_operonly_compat.so).
- Add remote D:lines. Note that these are not enabled by default.
- Remove EFnet-style G:lines. Noone appears to use these.
- Remove idle time checking (auto disconnecting users idle too long).
- Display a notice to clients when the IRCd is shut down using SIGTERM.
- Some error messages have been clarified to enhance usability.
- Close the link to servers that send invalid nicks (e.g. nicklen mismatches).
  Formerly the users were killed from the network.
- Enable topicburst by default in connect{}.
- Fix a potential desync which can happen with oper override.
- Remove "deopped" flag (TS5 legacy).
- Use 127.0.0.1 as nameserver if none can be found in /etc/resolv.conf.
- Only accept 127.0.0.x as a dnsbl listing.
- Change cloaking module (same as 2.2.1, different from 2.2.0).
- Make some more server notices about failed remote connect attempts
  network wide.
- Make some server notices about flooders and TS delta network wide.
- Remove redundant "<server> had been connected for <time>" server notice.
- Add resv oper privilege to control /resv, /unresv and cmode +L and +P,
  enabled by default.
- Add mass_notice oper privilege to control global notices and /wallops,
  enabled by default.
- Rework unkline/undline/unxline/unresv so they show the exact item removed
  and do not rehash bans.
- Show opers a list of recently (<24hrs) split servers in /map.
- Add /privs command, shows effective privileges of a client.

## charybdis-2.2.0

- The I/O code has been reworked, file descriptor metadata is stored in a
  hashtable and the maximum number of clients can now be set in ircd.conf.
- Improve error checking and error messages for kline/dline/xline/resv files.
- Allow kline ipv6:address, unkline some.host and unkline ipv6:address
  without *@.
- Add accountability (wallops, log) to OKICK extension.
- Add opernick to OPME/OMODE/OJOIN log messages.
- Add use_forward option, allows disabling cmode +fFQ and umode +Q.
- Add keyword substitution to DNSBL reasons, making it possible to show
  things like the user's IP address in the reason.
- Use sendto_one_notice() more.
- Server notices about kills now include the victim's nick!user@host instead
  of just nick.
- Include real hostname in Closing Link message for unknown connections
  that have sent USER, in particular banned users.
- Add some documentation about the SASL client protocol.
- Change spambot, flooder and jupe joiner notices from host to orighost.
- Remove the last remains of server hostmasking (this made it possible to
  have multiple servers with similar names appear as a single server).
- Keep bitmasks of modularized umodes reserved forever to the letter,
  avoiding problems when reloading umode modules in a different order.
- Fix -logfile.
- Update to the new revision (v8) of the TS6 spec, this fixes problems with
  joins reversing certain mode changes crossing them. This interoperates
  with older versions.
- Put "End of Channel Quiet List" at the end of +q lists.
- Fix invisible count getting desynched from reality if the act of opering
  up sets -i or +i.
- Don't leak auth{} spoofed IP addresses in +f notices.
- Shorten quit/part/kick reasons to avoid quit reasons overflowing the
  client exiting server notice (from TOPICLEN to 260).
- Fix some cases where 10 char usernames lose their final character.
- Move username check after xline and dnsbl checks, so it will not complain
  to opers about clients who are xlined or blacklisted anyway (both of
  which silently reject).
- Remove invite_ops_only config option, forcing it to YES.
- Allow /invite (but not invex) to override +r, +l, +j in addition to +i.
- Add several new extensions, such as createoperonly.
- Merge whois notice extensions into one and move it from snomask +y to +W.

## charybdis-2.1.2

- Fix bug that could cause all hostmangled users to be exempted when a
  single ban exception existed on a channel.
- Tweak \s code a little.
- Add a minor clarification to the SGML docs.
- Avoid truncation in ip_cloaking (by removing components on the other side).
  Note that this may cause channel +bqeI modes set on such very long hosts
  to no longer match.

## charybdis-2.1.1

- Search the shortest list (user's/channel's) when looking up channel
  memberships.
- Make the SID-collision notice look right under all conditions.
- Move kills from services from +s to +k snomask.
- When no_tilde is present on an auth{} block, check the non-tilde version
  of the user@host against k:lines as well.
- Put full reason in the SQUIT reason when a server is rejected for
  insufficient parameters being passed to a command.
- Don't redirect users to an existing domain, irc.fi.
- Improve communication of servlink-related error messages.

## charybdis-2.1.0

- Our official website is now http://www.ircd-charybdis.org/.
- Make RPL_ISUPPORT (005 numeric) modularizable.
- Also do forwarding if the channel limit (+l) is exceeded.
- Don't count opers on service{} servers in /lusers.
- Allow servers to send to @#chan and +#chan.
- Allow +S clients (services) to send to channels and @/+ channels always.
- Allow normal match() on IP address also in /masktrace.
- Add new testmask from ratbox 2.2. Allows matches on nick, ip and gecos
  in addition to user and host, and is fully analogous to masktrace.
  The numeric has changed from 724 to 727 and fields in it have changed.
- Show IP addresses to opers in /whowas.
- Add extb_extgecos extban option ($x:nick!user@host#gecos), from sorcery
  modules.
- Add extb_canjoin extban option ($j:#channel), matches if the user is banned
  from the other channel.
- Allow opers to /who based on realhost.
- Allow opers to /masktrace, /testmask based on realhost.
- Add general::operspy_dont_care_user_info, limits operspy accountability to
  channel-related information.
- Make host mangling more reliable.
- Prevent ban evasion by enabling/disabling host mangling.
- Add EUID, sends real host and services account in the same command as other
  user information.
- Make it possible to send CHGHOST without ENCAP (fixes problems with old
  services).
- Allow service{} servers to manipulate the nick delay table (for "nickserv
  enforcement", aka SVSHOLD).
- Send server notices about connections initiated by remote opers network wide.
- Fix too early truncation of JOIN channel list.
- Make the newconf system available to modules.
- Add /stats s to the hurt module to list active hurts.
- Add general::servicestring, shown in /whois for opered services (+oS).
- Show real host/IP behind dynamic spoof in /whois to the user themselves
  and opers.
- Document option to disable nick delay.
- Improve logging of server connections.
- Clean up handling of hostnames in connect blocks.
- Remove support for resolving ip6.int, people should be using ip6.arpa.
- Unbreak --disable-balloc (useful for debugging with tools like valgrind).
- Make Solaris 10 I/O ports code compile.
- Add WEBIRC module to allow showing the real host/IP of CGI:IRC users.
- Comment out blacklist{} block in example confs, as AHBL requires
  notification before use.
- Fix some bugs relating to the resolver.

## charybdis-2.0.0

- Replace ADNS with a new smaller resolver from ircu and hybrid.
- Make services shortcuts (/chanserv etc) configurable in ircd.conf.
- Add extban: extensible +bqeI matching via modules. Syntax is
  $<type>[:<data>]. By default no modules are loaded.
- Add DNS blacklist checking.
- Change operator{} block user@host from host to orighost. This means that
  services/+h spoofs do not work in operator{} blocks; auth{} spoofs still
  work. Check your operator{} blocks!
- Split contrib/ into extensions/ and unsupported/.
- Change CHGHOST do show the change to all other clients on common channels
  with quit/join/mode.
- Add /rehash nickdelay to clear out the nickdelay tables.
- Glines are now disabled in the example confs.
- Show more error messages on stderr.
- Add OMODE command to extensions/ for easier oper mode hacking.
- Add HURT system to extensions/; this shuns clients matching certain host/ip
  unless and until they identify to services. Mainly intended for SorceryNet.
- Show SASL success and failure counts in /stats t.
- Allow more frequent autoconnects to servers.
- Messaging services by nickname no longer uses target change slots.
- Only accept SASL from servers in a service{} block.
- New auth{} flag need_sasl to reject users who haven't done SASL
  authentication.
- Expand blah.blah and blah:blah to *!*@... instead of ...!*@* for bans
- Don't allow opers to fake locops/operwall to +w.
- Documentation updates.
- Many bugfixes.

## charybdis-1.1.0

- Implement SAFELIST.
- Incorporate ircu's match() algorithm.
- Improve usermode modularization.
- Seperate server notices into a seperate snomask, freeing up many
  usermodes to be used.
- Add support for SIGNON originating from Hyperion2.
- Modularize many server notices into seperate modules.
- Add hooks for can_join and can_create_channel.
- Add support for SASL authentication.
- Add introduce_user hook for adding new messages when a user is bursted.
- Move a large part of the ircd into libcharybdis.
- Don't complain "unknown user mode" if a user tries to unset
  a mode they do not have access to.
- Update our challenge specification to the challenge implementation in
  ratbox 2.2 for interoperability.
- Make +f notices network-wide (local host, global host,
  global user@host, local class), other notices tied to +f remain local.
- Allow ENCAP REALHOST outside of netburst.
- Add general::global_snotices option to make server notices be
  network-wide or not.
- Add sno_farconnect.c to contrib, provides farconnect support.
  Could be useful for BOPM.
- Add sno_routing.c which displays information about netsplits, netjoins
  and the clients affected by them.
- Add CHANTRACE and TRACEMASK commands from ratbox 3.0
- Use IsOperAdmin() instead of IsAdmin() when sending admin-only messages,
  that way hidden admins get them too.
- Add m_error to core_module_table, somehow it was missing.
- Correct a format string bug that occurs when a read error is
  received.
- Add some logging in places where we drop servers and only notify
  server operators.
- Track hostmask limits based on a client's original host, if
  available.
- Move HIDE_SPOOF_IPS into the general {} block in ircd.conf

## charybdis-1.0.3

- Fix /invite UID leak. (Found by logiclrd@EFnet.)
- Incorporate ratbox bugfixes for the MONITOR system.
- Made show_ip() less braindead.
- Show real errno if we fail to connect to a server.
- Don't disclose server IP's when a connection fails.
- Do not show the channels a service is sitting in.
- Reverted the aline code from hybrid-7.2
- Make sure TS6 services are recognized properly if connected remotely.
- Tweak something in services support for cyrix boxes.

## charybdis-1.0.2

- Fix propagation of an empty SJOIN (permanant channels).
- Fix an exploit involving a malformed /trace request.
- Don't display a blank RPL_WHOISCHANNELS in a remote whois request.
- Allow modules to provide new usermodes.
- On a nickname collision, change the collided nick to their unique ID,
  if general::collision_fnc is enabled in the config.
- Don't allow UID lookups in /monitor + and /monitor s
- Fix a garbage issue with channel mode +j.
- Apply proper capability flags to the proper server in me_gcap().
- Use find_named_person() instead of find_person() in a nick collision.
- Prevent UID disclosure in cmode setting.
- Prevent UID disclosure to remote clients in /kick.
- Do not allow users to query via /whois <server> <UID>.
- Don't allow local users to use UID's in local usermode changes.
- Propagate +q lists on netjunction.
- Clear +q lists on a lowerTS SJOIN.
- Ported a generic k/d/x-line parser from hybrid-7.2 which resulted in
  duplicate code reduction.
- Fix linebuf raw code to not truncate lines longer than 512 bytes;
  improves ziplink reliability on net junction.
- Use find_named_person() vs find_person() in services alias code.
- Fix issue where channel forwarding token can be lost on net junction.
- Fix empty channel desync issues involving +P.
- Remove unused non-ENCAP CHGHOST support.
- Use TS6 form for SQUIT wallops.
- Propagate nickname changes for remote clients in TS6 form if possible,
  even if sent in TS5 format.
- Only clear oper_only_umodes for local clients on deoper.

## charybdis-1.0.1

- Display logged in status on non-local clients too.
- Documentation updates
- Fix a bug with forward target authorization.
- Fix a bug with mode propagation (+Q/+F).
- Change ERR_NOSUCHNICK to ERR_SERVICESOFFLINE in services aliases.
- Add remote rehashing.
- Document service { } blocks (u:lines on ircu).
- Document identify_service and identify_command in reference.conf.

## charybdis-1.0

- Implement channel mode +L for channel list limit exemptions.
- Implement channel mode +P primarily as a status mode, permanant
  channel -- this is usually enforced via services registrations.
- Change behaviour of /stats p: now displays all staff members instead
  of local ones only.
- Make oper_list global, add local_oper_list for local traffic.
- Strip control codes from parts and quits.
- Add channel mode +c which strips control codes from messages sent to
  the channel.
- Add channel mode +g which enables free use of the /invite command.
- Add channel mode +z which sends rejected messages to channel ops.
  Could be useful for Q&A sessions or other similar events.
- Add channel quietmasks. These are recommended over the use of channel
  bans used to remove a user's ability to participate in the channel.
- Add channel join throttling mode, +j. Used to throttle channel join
  traffic, i.e. join/part flood attacks. Syntax: +j <joins>:<timeslice>
- Improvements to channel_modes(), from shadowircd -- allows for
  better construction of the mode string.
- Use the undernet throttle notice instead of bancache message when
  dealing with rejected clients. (stolen from ircu2.10.12)
- Add channel forwarding, via channel mode +f, behaves similarly to
  dancer-ircd version.
- Update example.conf to reflect AthemeNET changes. Original ratbox
  config is now reference.conf.
- Services account names are now tracked globally.
- Add channel mode +Q which disables the effects of channel forwarding
  on a temporary basis.
- Add channel mode +F which allows anybody to disable forwarding target
  authorisation, voluntarily on their channels.
- Make wallops behave like normal wallops.
- Add services aliases: /ns, /cs, /os, /nickserv, /chanserv, /operserv.
- Add simple hack that enables use of server password for automatic
  identify.
