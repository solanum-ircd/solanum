# TS6 protocol documentation

TS6 is the server-to-server protocol spoken by solanum and its predecessors
(e.g. charybdis, ratbox). This document aims to describe the protocol
comprehensively as it pertains to solanum. Other TS6-based IRCds may feature
their own extensions to the protocol which are not mentioned here.

## Concepts

The wire format is the same as the IRC client-to-server protocol, with a
limit of 15 parameters (not counting the prefix or command name) and no tags
portion unless both sides of the link have the **STAG** capability. If tags
are supplied, there is a limit of 30 tags. Byte limits are the same as the
client-to-server protocol. When sending messages, the prefix (origin) must be
a user or server that is reachable via you from the server you are sending it
to; you cannot spoof messages from "fake" directions. When solanum receives an
unrecognized non-`ENCAP` command from a server link, it will abort the link.

Both servers and users have IDs which remain stable for the life of their
connection. An *SID* (server ID) consists of a number followed by two
alphanumeric characters. The SID is defined in that server's configuration.
A *UID* (user ID) consists of the SID for the server that user is directly
connected to followed by six alphanumeric characters (so it is a total of nine
characters long). The first of those six alphanumeric characters should be a
letter; a number in that position is legal but reserved for future use. Both
SIDs and UIDs must be unique across the network. Historically, both SIDs and
UIDs are all-uppercase, although this is not a requirement of the protocol and
the IDs are case-sensitive.

When a server receives a command, unless it is intended for a local user, it
will generally need to propagate that command to other servers linked to it.
Since servers form a spanning tree, this is easily accomplished by sending
the command onwards to all linked servers *except* the server the command
was received from. The documentation for individual commands will describe
the expected propagation behavior for that command.

As part of the connection handshake, servers exchange the set of capabilities
that they support. Capabilities are a way to extend the base TS6 protocol with
additional commands. solanum requires certain capabilities in order to
successfully link with it. If a server lacks a particular capability, commands
gated behind that capability must not be sent to that server. If a server
receives an unknown command, it should terminate the link by disconnecting the
misbehaving server.

Many server protocol commands require that they are encapsulated via the
`ENCAP` command. This allows for broadcasting commands between servers without
requiring a specific server capability to be negotiated first. Servers pass
along `ENCAP` to all downstream servers matching its mask even if the server
itself does not recognize the encapsulated command. See the documentation for
`ENCAP` below for more details on how this works.

## Notation

This document refers to server capabilities in **BOLD** type and commands in
`MONOSPACE` type, to help distinguish between when a capability is named the
same as a command. When there is other important information, *emphasis* via
italics is used. For command parameters, `<angle brackets>` indicate a
required parameter and `[square brackets]` indicate optional text. Text
outside of brackets should be treated as those literal strings.

When defining propagation behavior, the following notations are used:

- Reply: The command is sent back to the locally-connected server sending us
  the command.
- Broadcast: The command is sent to all locally-connected servers *except* the
  one sending us the command.
- All servers: The command is sent to all locally-connected servers, including
  the one sending us the command (if any). This propagation type is uncommon
  and exists purely for legacy reasons.
- Target: The command is sent to the server corresponding to the target
  parameter (if the target is a client, the server the target is on); it does
  not need to be locally connected. The target should be a UID or SID, as
  determined by the command.
- Hunted server: The command is sent to the server corresponding to the hunted
  parameter; it does not need to be locally connected. The hunted parameter is
  a server or client name, potentially including * and ? wildcards, although
  only the first match is used if multiple servers match the mask. If the
  parameter is a client name, the server that client belongs to is used.
- Single server: The command is sent to the server corresponding to the server
  parameter; it does not need to be locally connected. The server parameter is
  a server name that does not include any wildcards.
- Server mask: The command is sent to all servers matching the server mask
  parameter; they do not need to be locally connected. If a server sent us the
  command, it is excluded from the mask evaluation. Intermediate servers
  should still propagate the command to downstream servers even if the
  intermediate does not match the server mask; it just doesn't process the
  command locally.
- None: The command is not propagated further.
- Special: The command documentation describes propagation behavior.

Propagation is evaluated independently by each server. In this way, a command
which specifies "broadcast" for its propagation behavior will eventually reach
the entire network, as each server will in turn send it across their local
segment of the network's spanning tree.

Capabilities and commands will have an implementation status of one of the
following values, indicating whether we recommend that servers or services
linking to solanum should support the capability or command:

- Required: support for it is required to be implemented
- Recommended: implementing it is highly recommended in general
- Optional: implementing it is optional depending on the needs of the network
- Legacy: exists for backwards compatibility with old IRCds; implementing it
  is not recommended
- Draft: based on a draft IRC specification and is subject to change in
  incompatible ways between now and if or when the draft is ratified;
  implementing it is optional
- Advertisement: the capability is not used for any S2S decisions and its
  presence simply indicates the existence of some feature. The feature itself
  is either mandatory or uses `ENCAP`, so the presence or absence of this
  capability in `CAPAB` is irrelevant

Solanum distinguishes between commands sent by a server as the source (prefix)
and commands sent by a user as the source. Each command will distinguish what
type of source is expected by each command. Sending the wrong type of source
will cause the command to be unrecognized over the S2S link and result in link
termination. Send the relevant UID or SID as the source rather than the
hostmask or server name.

## Timestamps

The protocol takes its name from the concept of timestamping nicknames and
channels. By attaching a timestamp to these events, it becomes possible to
resolve netjoin collisions in a deterministic manner such that "split riding"
into obtaining a nickname or manipulating a channel is not possible. While
various commands have a timestamp parameter, this section defines how to
handle collisions between nicknames, channel names, and other channel data
such as modes or topics.

### Nickname collision rules

Some commands accept a nick TS in order to detect race conditions between
servers regarding nickname usage. These commands are:

- `EUID`
- `NICK`
- `SIGNON`
- `UID`

A server receiving a command that requires nick TS rules must check for a
collision between an existing user and the nick in the received message (the
"new user"). The following table describes how to handle collisions depending
on the received TS of the new user compared to the local TS of the existing
user as well as the user@host of the existing user compared to the user@host
of the new user.

| TS Comparison  | user@host Comparison | Collide  |
|----------------|----------------------|----------|
| existing > new | identical            | new      |
| existing > new | different            | existing |                          
| existing = new | N/A                  | both     |
| existing < new | identical            | existing |
| existing < new | different            | new      |

There is one special case: if the existing user has not completed user
registration, the existing (unregistered) user is disconnected; ignore the
tables above and below. No propagation is needed because unregistered users
have not yet been introduced to the rest of the network.

If both servers support the **SAVE** capability (checked via `ENCAP GCAP` for
remote servers), then a `SAVE` message is generated for the colliding user.
This will cause a forced nick change to that user's UID. If both sides do not
support **SAVE**, then a `KILL` is issued instead. Consult the table below for
details on which servers the `SAVE` or `KILL` messages are propagated to.

| Colliding | Action | Propagation         |
|-----------|--------|---------------------|
| existing  | `SAVE` | All servers         |
| existing  | `KILL` | Broadcast           |
| new       | `SAVE` | Reply               |
| new       | `KILL` | Special (see below) |
| both      | `SAVE` | Special (see below) |
| both      | `KILL` | All servers         |

The case where the new user is being killed varies based on whether the
collision was caused by the new user changing their nick or if it was caused
by the new user being introduced to the network. If the collision is caused
by the new user changing their nick to an existing nickname, the `KILL` is
propagated to all downstream servers. Otherwise, it is propagated only to
the upstream server the message came from.

If both users are being saved, the existing user's `SAVE` (and subsequent
`NICK`) is propagated to all servers while the new user's `SAVE` is sent only
to the upstream server.

Note: Although `RSFNC` and `SVSLOGIN` takes nickname TS parameters, they do
not operate according to the above collision rules. See their documentation
for how they handle TS collisions with nicknames.

### Channel rules

Some commands require a channel TS in order to detect race conditions between
servers regarding channel usage. These commands are:

- `JOIN`
- `SJOIN`

If the TS received is lower than the local TS of the channel, the server must
remove all modes (channel modes, ban lists, and status modes). The new modes
and statuses are then accepted.

If the TS received is equal to our TS of the channel the server should keep
its current modes and accept the received modes and statuses.

If the TS received is higher than our TS of the channel the server should keep
its current modes and ignore the received modes and statuses. Any statuses
given in the received message will be removed.

### Simple channel rules

A simplified version of the channel rules is used for the following commands:

- `BMASK`
- `EBMASK`
- `ETB`
- `INVITE`
- `MLOCK`
- `TMODE`

If the TS received is higher than the TS of the local channel, the command is
ignored. Otherwise (TS received is equal to TS of local channel), the command
is processed. In general, none of these commands should receive a TS that is
lower than the local TS, as lowering the TS would be handled during `SJOIN` in
a netjoin burst or `JOIN` for regular channel creation races.

## Linking

Solanum allows both C2S and S2S connections on the same port. To introduce
yourself as a server, pass the following commands in order:

1. `PASS`
2. `CAPAB`
3. `SERVER`

The remote server will then respond with its own `PASS`, `CAPAB`, and `SERVER`
commands. After receiving `SERVER`, you should then send the following:

1. `SVINFO`
2. For each of your connected servers (local and remote):
    - `SID`
    - `ENCAP * GCAP` with that server's capability list
3. `BAN` for each of your propagated bans if the remote supports **BAN**
4. For each of your clients (local and remote):
    - `EUID` if the remote supports **EUID**, `UID` otherwise
    - `ENCAP * CERTFP` if the client has a certificate fingerprint
    - `ENCAP * REALHOST` if the remote does not support **EUID** and the
      client has a spoofed host
    - `ENCAP * LOGIN` if the remote does not support **EUID** and the client
      is logged into a services account
    - `AWAY` if the client is marked as away and bursting of away status is
      enabled in ircd.conf
    - `OPER` if the client is an oper
    - `ENCAP * IDENTIFIED` if the client is logged into a services account
5. For each of your channels:
    - `SJOIN` (repeated until all channel members are sent)
    - `EBMASK` for all list modes (+beIq) if the server supports **EBMASK**,
      `BMASK` otherwise (repeated until all mode lists are sent)
    - `TB` if a topic is set and the remote supports **TB**
    - `MLOCK` if the remote supports **MLOCK**
6. `PING :<remote>` where the remote parameter is the SID of the server being
   linked to

The remote will send all of the above as well, followed by a `PONG` to
acknowledge the `PING` you sent at the end. This `PONG` marks the end of the
remote server's burst. After receiving the remote server's `PING`, you must
reply with a `PONG` to signal that you are done bursting as well. This
completes the TS6 connection handshake and netjoin burst.

In order to successfully link, a remote server must support the following
capabilities:

- TS6 (support indicated via the `PASS` command rather than `CAPAB`)
- **ENCAP**
- **EX**
- **IE**
- **QS**

Servers lacking support for any of the above will be rejected and the link
dropped. Attempting to mix client and server protocol commands within a
connection will also cause your link to be dropped.

## Services

With the large amount of commands exposed to services, it may be difficult to
determine the correct ones to use in a given situation. This section aims to
provide a high-level overview of which services commands should be used in
order to set a user's account name and spoofed host (aka vHost or cloak).

Services servers should link according to the previous section, and should
support and advertise the **EBMASK**, **EOPMOD** (for `ETB`), **EUID**,
**MLOCK**, **TB**, and **STAG** capabilities. If the services package provides
features where services bots respond to in-channel commands it should
additionally support and advertise the **CHW** and **EOPMOD** (for STATUSMSG)
capabilities. If the services package provides the ability to set network bans
(e.g. AKILL) it should additionally support and advertise the **BAN**
capability. While optional, supporting and advertising **ECHO** will provide
better echo-message support for clients; if the services package does not
support **ECHO** then an echo-message will be generated from the solanum
server directly linked to services instead. Other optional capabilities are
not relevant when it comes to implementing services.

## Capabilities

Solanum supports the following server capabilities (enabled via `CAPAB`):

| Capability | Module    | Implementation | Description                                          |
|------------|-----------|----------------|------------------------------------------------------|
| BAN        | core      | recommended    | Propagated (global) KLINE/XLINE/RESV                 |
| CHW        | core      | recommended    | STATUSMSG @+ prefixes ("channel walls")              |
| CLUSTER    | core      | legacy         | Remote (UN)RESV/(UN)XLINE (not using ENCAP)          |
| EBMASK     | core      | recommended    | EBMASK command (burst +beIq with ts/setter data)     |
| ECHO       | m_message | recommended    | ECHO command (for echo-message)                      |
| ENCAP      | core      | required       | ENCAP command                                        |
| EOPMOD     | core      | recommended    | STATUSMSG = prefix (+z applied to +bq), ETB command  |
| EUID       | core      | recommended    | EUID command (UID + realhost/login info)             |
| EX         | core      | required       | Channel mode +e (ban exemptions)                     |
| IE         | core      | required       | Channel mode +I (invite exemptions)                  |
| KLN        | core      | legacy         | Remote KLINE (not using ENCAP)                       | 
| KNOCK      | core      | optional       | KNOCK command (request invite to channel)            |
| MLOCK      | core      | recommended    | MLOCK command (force channel modes set/unset)        |
| QS         | core      | required       | Quit storms (no need to send recursive SQUIT)        |
| REMOVE     | m_remove  | optional       | REMOVE command (force part)                          |
| RSFNC      | core      | advertisement  | ENCAP RSFNC (force nick change)                      |
| RSFNCF     | core      | advertisement  | Additional parameter to ENCAP RSFNC to override RESV |
| SAVE       | core      | recommended    | SAVE (force nick change on nick collision)           |
| SERVICE    | core      | advertisement  | ENCAP SU/LOGIN, channel mode +r                      |
| STAG       | core      | recommended    | Message tags                                         |
| TB         | core      | recommended    | TB (topic burst)                                     | 
| UNKLN      | core      | legacy         | Remote UNKLINE (not using ENCAP)                     |

## Commands

This section contains all commands that may be sent on S2S links. Commands
which are not propagated S2S are not included here. The table below groups
commands by rough functional areas, and each command is described in more
detail in subsections in alphabetical order. Commands which are sent over
`ENCAP` have an (E) suffix after the command name.

| Group         | Command          | Module               | Capability | Implementation |
|---------------|------------------|----------------------|------------|----------------|
| Accounts      | IDENTIFIED (E)   | m_identified         | ENCAP      | legacy         |
| Accounts      | CERTFP (E)       | m_certfp             | ENCAP      | recommended    |
| Accounts      | LOGIN (E)        | m_services           | ENCAP      | legacy         |
| Accounts      | MECHLIST (E)     | m_sasl               | ENCAP      | recommended    |
| Accounts      | SASL (E)         | m_sasl               | ENCAP      | recommended    |
| Accounts      | SIGNON           | m_signon             | -          | required       |
| Accounts      | SU (E)           | m_services           | ENCAP      | required       |
| Accounts      | SVSLOGIN (E)     | m_signon             | ENCAP      | required       |
| Bans          | BAN              | m_ban                | BAN        | recommended    |
| Bans          | DLINE (E)        | m_dline              | ENCAP      | optional       |
| Bans          | KLINE            | m_kline              | KLN        | legacy         |
| Bans          | KLINE (E)        | m_kline              | ENCAP      | optional       |
| Bans          | QUARANTINE (E)   | m_quarantine         | ENCAP      | optional       |
| Bans          | RESV             | m_resv               | CLUSTER    | legacy         |
| Bans          | RESV (E)         | m_resv               | ENCAP      | optional       |
| Bans          | UNDLINE (E)      | m_dline              | ENCAP      | optional       |
| Bans          | UNKLINE          | m_kline              | UNKLN      | legacy         |
| Bans          | UNKLINE (E)      | m_kline              | ENCAP      | optional       |
| Bans          | UNQUARANTINE (E) | m_quarantine         | ENCAP      | optional       |
| Bans          | UNRESV           | m_resv               | CLUSTER    | legacy         |
| Bans          | UNRESV (E)       | m_resv               | ENCAP      | optional       |
| Bans          | UNXLINE          | m_xline              | CLUSTER    | legacy         |
| Bans          | UNXLINE (E)      | m_xline              | ENCAP      | optional       |
| Bans          | XLINE            | m_xline              | CLUSTER    | legacy         |
| Bans          | XLINE (E)        | m_xline              | ENCAP      | optional       |
| Channels      | BMASK            | m_mode               | -          | legacy         |
| Channels      | EBMASK           | m_mode               | EBMASK     | recommended    |
| Channels      | ETB              | m_tb                 | EOPMOD     | recommended    |
| Channels      | INVITE           | m_invite             | -          | required       |
| Channels      | INVITED (E)      | invite_notify        | ENCAP      | recommended    |
| Channels      | JOIN             | m_join               | -          | required       |
| Channels      | KICK             | m_kick               | -          | required       |
| Channels      | KNOCK            | m_knock              | KNOCK      | optional       |
| Channels      | MLOCK            | m_mode               | MLOCK      | recommended    |
| Channels      | PART             | m_part               | -          | required       |
| Channels      | REMOVE           | m_remove             | REMOVE     | optional       |
| Channels      | SJOIN            | m_join               | -          | required       |
| Channels      | TB               | m_tb                 | TB         | recommended    |
| Channels      | TMODE            | m_mode               | -          | required       |
| Channels      | TOPIC            | m_topic              | -          | required       |
| Informational | ADMIN            | m_admin              | -          | recommended    |
| Informational | ETRACE (E)       | m_etrace             | ENCAP      | recommended    |
| Informational | INFO             | m_info               | -          | optional       |
| Informational | LINKS            | m_links              | -          | optional       |
| Informational | LUSERS           | m_lusers             | -          | recommended    |
| Informational | MOTD             | m_motd               | -          | recommended    |
| Informational | PRIVS (E)        | m_privs              | ENCAP      | recommended    |
| Informational | STATS            | m_stats              | -          | recommended    |
| Informational | TIME             | m_time               | -          | optional       |
| Informational | TRACE            | m_trace              | -          | recommended    |
| Informational | USERS            | m_users              | -          | recommended    |
| Informational | VERSION          | m_version            | -          | recommended    |
| Informational | WHOIS            | m_whois              | -          | required       |
| Informational | WHOWAS           | m_whowas             | -          | recommended    |
| Linking       | CAPAB            | m_capab              | -          | required       |
| Linking       | CONNECT          | m_connect            | -          | optional       |
| Linking       | GCAP (E)         | m_capab              | ENCAP      | recommended    |
| Linking       | PASS             | m_pass               | -          | required       |
| Linking       | SERVER           | m_server             | -          | required       |
| Linking       | SID              | m_server             | -          | required       |
| Linking       | SQUIT            | m_squit              | -          | required       |
| Linking       | SVINFO           | m_svinfo             | -          | required       |
| Logging       | ERROR            | m_error              | -          | required       |
| Logging       | OPERSPY (E)      | m_operspy            | ENCAP      | recommended    |
| Logging       | SNOTE (E)        | m_snote              | ENCAP      | recommended    |
| Logging       | TGINFO (E)       | m_tginfo             | ENCAP      | recommended    |
| Maintenance   | DIE (E)          | m_die                | ENCAP      | optional       |
| Maintenance   | MODLIST (E)      | m_modules            | ENCAP      | optional       |
| Maintenance   | MODLOAD (E)      | m_modules            | ENCAP      | optional       |
| Maintenance   | MODRELOAD (E)    | m_modules            | ENCAP      | optional       |
| Maintenance   | MODRESTART (E)   | m_modules            | ENCAP      | optional       |
| Maintenance   | MODUNLOAD (E)    | m_modules            | ENCAP      | optional       |
| Maintenance   | REHASH (E)       | m_rehash             | ENCAP      | optional       |
| Maintenance   | RESTART (E)      | m_restart            | ENCAP      | optional       |
| Maintenance   | SHEDDING (E)     | m_shedding           | ENCAP      | optional       |
| Messaging     | ADMINWALL (E)    | m_adminwall          | ENCAP      | optional       |
| Messaging     | ECHO             | m_message            | ECHO       | recommended    |
| Messaging     | NOTICE           | m_message            | -          | required       |
| Messaging     | OPERWALL         | m_wallops            | -          | optional       |
| Messaging     | PRIVMSG          | m_message            | -          | required       |
| Messaging     | TAGMSG           | m_message            | STAG       | recommended    |
| Messaging     | WALLOPS          | m_wallops            | -          | required       |
| Miscellaneous | ACK (E)          | cap_labeled_response | ENCAP      | recommended    | 
| Miscellaneous | BATCH            | m_batch              | STAG       | optional       |
| Miscellaneous | ENCAP            | m_encap              | ENCAP      | required       |
| Miscellaneous | PING             | m_ping               | -          | required       |
| Miscellaneous | PONG             | m_pong               | -          | required       |
| Miscellaneous | SETFILTER (E)    | filter               | ENCAP      | recommended    |
| Operators     | DEHELPER (E)     | helpops              | ENCAP      | optional       |
| Operators     | EXTENDCHANS (E)  | m_extendchans        | ENCAP      | optional       |
| Operators     | GRANT (E)        | m_grant              | ENCAP      | optional       |
| Operators     | OPER             | m_oper               | -          | recommended    |
| Operators     | OPER (E)         | m_oper               | ENCAP      | legacy         |
| Users         | AWAY             | m_away               | -          | required       |
| Users         | CHGHOST          | m_chghost            | EUID       | recommended    |
| Users         | CHGHOST (E)      | m_chghost            | ENCAP      | legacy         |
| Users         | EUID             | m_nick               | EUID       | recommended    |
| Users         | KILL             | m_kill               | -          | required       |
| Users         | MODE             | m_mode               | -          | required       |
| Users         | NICK             | m_nick               | -          | required       |
| Users         | NICKDELAY (E)    | m_services           | ENCAP      | recommended    |
| Users         | REALHOST (E)     | m_chghost            | ENCAP      | legacy         |
| Users         | RSFNC (E)        | m_services           | ENCAP      | required       |
| Users         | QUIT             | m_quit               | -          | required       |
| Users         | SAVE             | m_nick               | SAVE       | recommended    |
| Users         | UID              | m_nick               | -          | legacy         |

### ACK (E)

- Capability: **ENCAP**
- Source: server
- Propagation: single server
- Implementation: recommended
- Syntax: `ENCAP <server> ACK`

Acknowledges a command which returned no response when a labeled-response was
requested for a remote command via the solanum.chat/response tag. The `ACK`
must forward the solanum.chat/response tag back to the server which originated
the command.

### ADMIN

- Capability: none
- Source: user
- Propagation: hunted server
- Implementation: recommended
- Syntax: `ADMIN :<hunted>`

Remote `ADMIN` request. Replies to the source with administrative information
about the server.

### ADMINWALL (E)

- Capability: **ENCAP**
- Source: user
- Propagation: broadcast
- Implementation: optional
- Syntax: `ENCAP * ADMINWALL :<message>`

Sends a `WALLOPS` to all administrators (user mode +a).

### AWAY

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: required
- Syntax: `AWAY :<message>`

Marks the user as away with the given away message. The message parameter
cannot be an empty string.

----

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: required
- Syntax: `AWAY`

Marks the user as no longer away.

----

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: legacy
- Syntax: `AWAY :`

Marks the user as no longer away.

### BAN

- Capability: **BAN**
- Source: any
- Propagation: broadcast
- Implementation: recommended
- Syntax: `BAN <type> <user mask> <host mask> <creation TS> <duration> <lifetime> <oper> :<reason>`

Propagates a network-wide ban. This command is used to create new bans, remove
bans, and change properties of existing bans.

Parameters:
- type: The type of ban (single letter):
    - `K`: KLINE
    - `R`: RESV
    - `X`: XLINE
- user mask: If type is `K`, the user portion of the k-line. Otherwise, this
  should be a literal `*`.
- host mask: If type is `K`, the host portion of the k-line. Otherwise, this
  contains the full ban mask.
- creation TS: TS of when the ban was last modified.
- duration: Minutes after creation TS this ban is valid for. 0 to remove.
- lifetime: Minutes after creation TS this ban should be tracked and
  propagated by the server.
- oper: Oper UID who set the ban or `*`.
- reason: Ban reason, optionally with a private oper-facing reason.

When changing properties of an existing ban, the incoming `BAN` must be
ignored and not propagated if the creation TS of the incoming ban is older
than the creation TS of the existing ban. If the incoming ban's creation TS is
identical to the existing ban's creation TS and the incoming ban's lifetime is
less than the existing ban's lifetime, it should not be propagated to avoid
unnecessary network traffic. Two changes to bans that set the TS to the same
value may cause desynchronization.

When the duration has passed, the ban is no longer active but it may still
be necessary to remember it. Setting the duration to 0 removes a ban.

The lifetime indicates for how long this ban needs to be remembered and
propagated. This *must* be at least the duration. Initially, it is usually set
the same as the duration but when the ban is modified later, it must be set
such that the modified ban is remembered at least as long as the original ban.
This ensures that the original ban does not revive via split servers.

The oper field indicates the oper that originally set the ban. If this message
is creating a new ban, it *should* be sent as a literal `*` instead.

The reason field indicates the reason for the ban. Any part after a literal
`|` *must not* be shown to normal users.

### BATCH

- Capability: varies
- Source: varies
- Propagation: special
- Implementation: optional
- Syntax: `BATCH +<id> <type> [<params...>]`

Begins a new S2S batch. This command *must not* be sent unless a capability
enabling the specific batch type chosen is enabled by both sides of the link.
See the Batches section below for more information on supported batch types,
the capabilities they need, the syntax for their params, and any source and
propagation behavior needed by that type.

In addition to the capability needed by individual batch types, both sides of
the link must additionally support **STAG** in order to properly send messages
inside of the batch.

----

- Capability: varies
- Source: varies
- Propagation: special
- Implementation: optional
- Syntax: `BATCH -<id>`

Closes a S2S batch. The batch must have previously been opened with the same
id parameter by the server.

### BMASK

- Capability: none
- Source: server
- Propagation: broadcast
- Implementation: legacy
- Syntax: `BMASK <channel TS> <channel> <mode> :<masks...>`

Propagates a list mode during a netjoin burst. It is highly recommended to use
`EBMASK` instead as that command retains the time the modes were set and who
set them. This command follows the simplified channel TS rules.

Parameters:
- channel TS: The TS of the channel being bursted
- channel: Channel name
- mode: One of `b`, `e`, `I`, or `q` to indicate the mode being bursted
- masks: Space-separated list of masks to set for that mode

All list modes must be burst with either this command or `EBMASK` during a
netjoin, not `MODE` or `TMODE`.

### CAPAB

- Capability: none
- Source: server
- Propagation: none
- Implementation: required
- Syntax: `CAPAB :<caps...>`

Send the list of capabilities this server supports. This must be sent exactly
once, during the initial server handshake. Sending `CAPAB` a second time is
not allowed and will drop the link. The caps parameter is a space-separated
list of capability names.

### CERTFP (E)

- Capability: **ENCAP**
- Source: user
- Propagation: broadcast
- Implementation: recommended
- Syntax: `ENCAP * CERTFP :<fingerprint>`

Sets the TLS client certificate fingerprint for the source user. This is
exposed in `WHOIS` replies to opers and is useful for services servers to
automatically log users in based on their certificate fingerprints in the
event that SASL was not used. The fingerprint parameter should be non-empty;
do not send this command if the user does not have a certificate fingerprint.
This command does not pass the hashing method used to derive the fingerprint
and each server on the network must agree on that hashing method in order for
the fingerprint to be useful.

### CHGHOST

- Capability: **EUID**
- Source: any
- Propagation: broadcast
- Implementation: recommended
- Syntax: `CHGHOST <target> <host>`

Sets the target's hostname spoof to the host parameter. This command is only
capable of spoofing the host and cannot spoof the username (ident). Setting
the host parameter to the target's original host removes the spoofed host.

The host parameter must meet the following criteria to be considered valid,
otherwise the host change will be rejected:

- Host cannot be an empty string
- Host cannot begin with `:`
- Host can only contain the characters A-Z, a-z, 0-9, and `-./:`; notably,
  underscores and formatting codes are *not* allowed in hostnames
- A number cannot immediately follow the final `/` in the host; this prevents
  potential ambiguity with CIDR notation in bans

When changing the hostname to an IPv6 address, the ip cannot start with `:`
(as this would otherwise be interpreted as the beginning of the trailing
parameter in IRC protocol framing). To work around this, prefix the address
with a leading 0, e.g. `0::1` for localhost.

When propagating this command, if a server does not support **EUID**, it will
be propagated via the `ENCAP` variant below instead. However, supporting
**EUID** and handling this command directly is superior as it ensures that no
intermediate servers ignore the changed hostname and cause a network desync.

### CHGHOST (E)

- Capability: **ENCAP**
- Source: any
- Propagation: broadcast
- Implementation: legacy
- Syntax: `ENCAP * CHGHOST <target> :<host>`

This operates identically to the regular `CHGHOST` command described above,
but is used for servers that do not support **EUID**.

### CONNECT

- Capability: none
- Source: any
- Propagation: hunted server
- Implementation: optional
- Syntax: `CONNECT <servername> <port> :<hunted>`

Remotely connects the hunted server to the specified servername on the
specified port. The port parameter may be 0 to use the default port as defined
in the connect{} block for servername in the hunted server's ircd.conf. This
command will fail if no connect{} block for servername exists in the hunted
server's ircd.conf. It will also fail if the specified servername already
exists on the network.

### DEHELPER (E)

- Capability: **ENCAP**
- Source: user
- Propagation: single server
- Implementation: optional
- Syntax: `ENCAP <server> DEHELPER <target>`

If the helpops extension is loaded, removes the +h user mode from target so
they no longer appear on `STATS p`. The target must be locally connected to
the server designated in the server parameter.

### DIE (E)

- Capability: **ENCAP**
- Source: user
- Propagation: single server
- Implementation: optional
- Syntax: `ENCAP <server> DIE <server>`

Remotely terminates the specified server. The server parameter must exactly
match the remote server's name, with no wildcard components, in order for the
server to terminate.

### DLINE (E)

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `ENCAP <server mask> DLINE <duration> <ip> :<reason>`

Sets a d-line (IP or CIDR ban) on matching servers. D-lines are not propagated
bans and do not get sent to remote servers with the `SENDBANS` command. As
such, long-lived d-lines intended to be network-wide are not recommended due
to the difficulty in ensuring that each new server that joins the network gets
them set to avoid "holes" in the ban.

Parameters:

- server mask: The server mask (including wildcards) to set the d-line on.
- duration: The duration in minutes of the d-line. A duration of 0 indicates a
  permanent d-line.
- ip: The IP address or CIDR to ban. Wildcards are not allowed.
- reason: The reason for the ban, including an optional oper-facing reason.

When banning an IPv6 address or CIDR, the ip cannot start with `:` (as this
would otherwise be interpreted as the beginning of the trailing parameter in
IRC protocol framing). To work around this, prefix the address with a leading
0, e.g. `0::1` to ban localhost.

The reason field indicates the reason for the ban. Any part after a literal
`|` *must not* be shown to normal users.

### EBMASK

- Capability: **EBMASK**
- Source: server
- Propagation: broadcast
- Implementation: recommended
- Syntax: `EBMASK <channel TS> <channel> <mode> :(<mask> <when> <who>)...`

Propagates a list mode during a netjoin burst, including information about the
time the entry was originally set and who set it. The final parameter is a
space-separated list of triplets containing this information. This command
follows the simplified channel TS rules.

Parameters:
- channel TS: The TS of the channel being bursted
- channel: Channel name
- mode: One of `b`, `e`, `I`, or `q` to indicate the mode being bursted
- mask: Mask to add to the mode's list
- when: Time the mask was set, as a unix timestamp
- who: Hostmask of the user who set the mask or server name if set by a server

All list modes must be burst with either this command or `BMASK` during a
netjoin, not `MODE` or `TMODE`.

### ECHO

- Capability: **ECHO**
- Source: user
- Propagation: target
- Implementation: recommended
- Syntax: `ECHO <type> <target> :<message>`

Sends a copy of the target's message back to that target, for the echo-message
client capability. The source is the message's original recipient.

Parameters:
- type: `P` for PRIVMSG, `N` for NOTICE
- target: UID of the original message's sender
- message: The message that was delivered to the original message's target,
  potentially modified server-side by hooks or intermediate ircds

To ensure the text echoed is as accurate as possible, the `ECHO` message is
generated by the server the original message's target is connected to. If any
server along the path does not support the **ECHO** capability, then the
furthest-along server will generate the `ECHO` message instead.

`ECHO` is used only for messages delivered to clients (aka PMs); channel
messages will always generate an echo-message from the local server instead of
being propagated s2s via `ECHO`.

For servers that additionally support **STAG**, the `ECHO` should have every
tag that was delivered to the original message's target attached to it.

----

- Capability: both **ECHO** and **STAG**
- Source: user
- Propagation: target
- Implementation: recommended
- Syntax: `ECHO T <target>`

Variation of `ECHO` used for TAGMSG. Because TAGMSG lacks text, this variant
does not have a final trailing parameter. The `ECHO` should have every tag
that was delivered to the original message's target attached to it. This
variant otherwise follows all of the same rules as the variant for PRIVMSG and
NOTICE.

### ENCAP

- Capability: **ENCAP**
- Source: any
- Propagation: server mask
- Implementation: required
- Syntax: `ENCAP <server mask> <command> [<params...>]`

This command is used to encapsulate another command for propagation to other
servers. Because solanum will close server links upon receiving unknown
commands, `ENCAP` provides an easy way to optionally extend the protocol via
loaded modules without needing to define new capabilities. If a recipient
server matches the mask but does not understand the encapsulated command, it
will be silently ignored by that server but the `ENCAP` will still be
propagated to downstream servers according to the provided server mask.

Servers which do not match the server mask still need to propagate the `ENCAP`
to downstream servers. They will not otherwise process the command.

### ERROR

- Capability: none
- Source: server
- Propagation: none
- Implementation: required
- Syntax: `ERROR :<message>`

This command indicates an error condition arose and is sent immediately before
disconnecting the other server.

### ETB

- Capability: **EOPMOD**
- Source: any
- Propagation: broadcast
- Implementation: recommended
- Syntax: `ETB <channel TS> <channel> <topic TS> <who> :<topic>`

Bursts a topic change. If the channel TS provided is lower than our TS for the
channel, the topic change is always applied, regardless of the topic TS. If
the channel TS provided matches our channel TS, the topic change is only
applied if the topic TS provided is *greater* than our own topic TS. This
command follows the simplified channel TS rules.

Parameters:
- channel TS: The channel's TS
- channel: Channel name
- topic TS: Unix timestamp of when the topic was set
- who: Hostmask of the topic setter or server name if set by a server
- topic: Updated channel topic (may be an empty string)

The topic may be set to the same as the existing topic if only the metadata
(topic TS and who) needs to be propagated to other servers. When propagating,
if a downstream server does not support **EOPMOD**, it will be propagated as
either `TB` (if the downstream server supports **TB**, the new topic is
non-empty, and the topic is currently either unset or the topic TS is greater
than the current topic's TS) or `TOPIC` instead. When performing this fallback
propagation, the who parameter must be a user rather than a server. Fallback
propagation for server-set topics is not supported and will cause a network
desync as the topic change will only be propagated to some servers and not
others. Additionally, when performing fallback propagation, the ircd will
synthesize an `SJOIN` for the topic setter before sending `TB`/`TOPIC` and a
`PART` for them afterwards if they were not already on the channel.

### ETRACE (E)

- Capability: **ENCAP**
- Source: user
- Propagation: single server
- Implementation: recommended
- Syntax: `ENCAP <server> ETRACE <target>`

Performs an enhanced trace on the target user, which must be locally connected
to the server the command is propagated to. The reply will consist of a
`RPL_ETRACEFULL` (708) numeric followed by `RPL_ENDOFTRACE` (262). These
numerics are defined as follows:

```
262 <sender> <server> :End of TRACE
708 <sender> <oper> <class> <nick> <username> <host> <ip> <user1> <user2> :<realname>
```

Numeric parameters:

- sender: Nickname of the user sending `ETRACE`
- server: Name of the server responding to the `ETRACE`
- oper: The literal string "Oper" if target is an oper and "User" otherwise
- class: Target's connection class (class{} block in ircd.conf, aka Y-line)
- nick: Target's nickname
- username: Target's username
- host: Target's hostname
- ip: Target's IP address ("255.255.255.255" if a hostname spoof is active,
  the sender and target are different users, and the sender lacks the
  auspex:hostname oper priv)
- user1: Value of the target's second `USER` parameter ("\<hidden>" if a
  hostname spoof is active, the sender and target are different users, and the
  sender lacks the auspex:hostname oper priv)
- user2: Value of the target's third `USER` parameter ("\<hidden>" if a
  hostname spoof is active, the sender and target are different users, and the
  sender lacks the auspex:hostname oper priv)
- realname: Target's realname

### EUID

- Capability: **EUID**
- Source: server
- Propagation: broadcast
- Implementation: recommended
- Syntax: `EUID <nick> <hops> <nick TS> <modes> <user> <host> <ip> <uid> <realhost> <account> :<realname>`

Introduces a new user to the network. This command is an improvement over
`UID` by including real host and services account information, condensing
bursts from three commands per user (`UID`, `ENCAP REALHOST`, `ENCAP LOGIN`)
to one. This command follows nickname TS rules.

Parameters:

- nick: The user's nickname
- hops: Number of hops (including the destination server, so minimum 1) it
  takes to reach the client through this link
- nick TS: The user's timestamp
- modes: User modes for the user, with leading "+" (if the user has no user
  modes, just a literal "+")
- user: The user's username
- host: The user's visible hostname
- ip: The user's IP address, or literal "0" if user has an IP spoof
- uid: The user's UID
- realhost: The user's real (non-spoofed) hostname, or literal "*" if the host
  parameter already contains the real hostname
- account: The user's services account username, or literal "*" if the user is
  not logged into a services account
- realname: The user's realname

### EXTENDCHANS (E)

- Capability: **ENCAP**
- Source: user
- Propagation: single server
- Implementation: optional
- Syntax: `ENCAP <server> EXTENDCHANS <target>`

Increases the maximum number of channels the target can join. For solanum, the
new limit is determined by the max_chans_per_user_large setting in ircd.conf.
The target server determines the new limit, rather than the sending server.

### GCAP (E)

- Capability: **ENCAP**
- Source: server
- Propagation: broadcast
- Implementation: recommended
- Syntax: `ENCAP * GCAP :<caps...>`

Advertises the source's server capabilities to the rest of the network. This
command must be sent once when the server is first introduced and cannot be
sent again for that server. The caps parameter is a space-separated list of
capability names.

### GRANT (E)

- Capability: **ENCAP**
- Source: user
- Propagation: single server
- Implementation: optional
- Syntax: `ENCAP <server> GRANT <target> <privset>`

Opers the target with the specified privset. The target user must be locally
connected to the target server and the privset must exist on that server.

----

- Capability: **ENCAP**
- Source: user
- Propagation: single server
- Implementation: optional
- Syntax: `ENCAP <server> GRANT <target> deoper`

Revokes oper status from the target, who must be locally connected to the
target server.

### IDENTIFIED (E)

- Capability: **ENCAP**
- Source: server
- Propagation: broadcast
- Implementation: legacy
- Syntax: `ENCAP * IDENTIFIED <target> <nick> [OFF]`

Marks the target as identified to the services account that owns the target's
current nickname. This has no effect on anything in solanum outside of the
vendor solanum.chat/identify-msg client capability (e.g. it does not allow the
target to join a +r channel or speak in a +R channel).

The nick parameter must match the target's current nickname or the command
will be ignored. If the final parameter is set to the literal string "OFF", it
marks the user as no longer identified to the account that owns their current
nickname.

If you are writing a services package, use `ENCAP SU` and `ENCAP SVSLOGIN`
to propagate the actual services account name as well as this command.

### INFO

- Capability: none
- Source: user
- Propagation: hunted server
- Implementation: optional
- Syntax: `INFO <hunted>`

Displays information about the server to the source user.

### INVITE

- Capability: none
- Source: user
- Propagation: target
- Implementation: required
- Syntax: `INVITE <target> <channel> <channel TS>`

Invites target to the specified channel. This command follows simple channel
TS rules.

### INVITED (E)

- Capability: **ENCAP**
- Source: server
- Propagation: broadcast
- Implementation: recommended
- Syntax: `ENCAP * INVITED <sender> <target> <channel>`

Notifies the network that sender invited target into the specified channel, to
support the invite-notify client capability.

### JOIN

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: required
- Syntax: `JOIN <channel TS> <channel> +`

Joins a user to a channel. The channel does not need to already exist first,
however for new channel creation it is better to send `SJOIN` instead of
`JOIN` as `SJOIN` allows you to op the channel creator and set default channel
modes in the same command rather than requiring a separate `MODE` command.

Historically, the third parameter specified modes. This is no longer used and
should be sent as a literal "+" instead.

This command follows channel TS rules.

----

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: required
- Syntax: `JOIN 0`

Causes the user to part all channels.

### KICK

- Capability: none
- Source: any
- Propagation: broadcast
- Implementation: required
- Syntax: `KICK <channel> <target> :<comment>`

Kicks a user from a channel.

### KILL

- Capability: none
- Source: any
- Propagation: broadcast
- Implementation: required
- Syntax: `KILL <target> :<path> (<reason>)`

Removes a user from the network. The path parameter indicates the source of
the kill; for servers, it is the server name. For users, it is a "!"-separated
listing of server name, user's hostname, user's username, and user's nick. The
path is relayed as-is without modification or validation; while RFC 1459
recommends prepending the current server to the path, doing this could cause
the message to exceed the 512-byte limit. As such, it is best to not
manipulate the path. External (non-solanum) servers may use their own
formatting for the path parameter provided that it does not contain spaces;
there is no requirement that the path be any specific format.

The reason parameter is surrounded by parenthesis by convention. This is not a
strict requirement of the protocol and the parenthesis may be omitted.
However, it is best to include them for consistency with solanum-generated
kill messages.

### KLINE

- Capability: **KLN**
- Source: user
- Propagation: server mask
- Implementation: legacy
- Syntax: `KLINE <server mask> <duration> <user> <host> :<reason>`

Sets a k-line on matching servers. The duration is in minutes, with a value of
0 indicating a permanent ban. The user and host parameters may contain
wildcards, and the host parameter may also be in CIDR notation.

When banning an IPv6 address or CIDR, the host cannot start with `:` (as this
would otherwise be interpreted as the beginning of the trailing parameter in
IRC protocol framing). To work around this, prefix the address with a leading
0, e.g. `0::1` to ban localhost.

The reason field indicates the reason for the ban. Any part after a literal
`|` *must not* be shown to normal users.

If a k-line for the user and host already exists, this command will be ignored
locally (but still propagated) unless the new k-line has a longer duration
than the previous one. When propagating this command, send `KLINE` to servers
supporting **KLN** and `ENCAP KLINE` to other servers.

Note: This command is for setting server-specific k-lines that are not further
propagated as new servers join the network. Prefer using `BAN` instead.

### KLINE (E)

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `ENCAP <server mask> KLINE <duration> <user> <host> :<reason>`

Sets a k-line on matching servers. The duration is in minutes, with a value of
0 indicating a permanent ban. The user and host parameters may contain
wildcards, and the host parameter may also be in CIDR notation.

When banning an IPv6 address or CIDR, the host cannot start with `:` (as this
would otherwise be interpreted as the beginning of the trailing parameter in
IRC protocol framing). To work around this, prefix the address with a leading
0, e.g. `0::1` to ban localhost.

The reason field indicates the reason for the ban. Any part after a literal
`|` *must not* be shown to normal users.

If a k-line for the user and host already exists, this command will be ignored
unless the new k-line has a longer duration than the previous one.

Note: This command is for setting server-specific k-lines that are not further
propagated as new servers join the network. Prefer using `BAN` instead.

### KNOCK

- Capability: **KNOCK**
- Source: user
- Propagation: broadcast
- Implementation: optional
- Syntax: `KNOCK <channel>`

Propagates a knock request from a user to the specified channel.

### LINKS

- Capability: none
- Source: user
- Propagation: hunted server
- Implementation: optional
- Syntax: `LINKS <hunted> :<mask>`

Displays the linked servers on the network from the perspective of the hunted
server. The mask parameter filters which servers are displayed and may contain
wildcards. It may be an empty string, in which case no filter is applied.

### LOGIN (E)

- Capability: **ENCAP**
- Source: user
- Propagation: broadcast
- Implementation: legacy
- Syntax: `ENCAP * LOGIN <account>`

Sets the user's services account name to the specified account. This command
is used during netjoin bursts to servers that do not support **EUID** and
should not be directly sent by services. If you are writing a services
package, use `ENCAP SU` and `ENCAP SVSLOGIN` instead to let the network know
that a user logged in or out.

### LUSERS

- Capability: none
- Source: user
- Propagation: hunted server
- Implementation: recommended
- Syntax: `LUSERS * :<hunted>`

Shows statistics for the number of local and global users and channels on the
hunted server to the source user. The first parameter is unused and ignored.

### MECHLIST (E)

- Capability: **ENCAP**
- Source: server
- Propagation: broadcast
- Implementation: recommended
- Syntax: `ENCAP * MECHLIST :<mechs...>`

Sent by services to indicate the SASL mechanisms it supports. The mechs
parameter is a comma-separated list of mechanism names that should be
advertised to clients as the value for the sasl capability in `CAP LS 302`.
Solanum does not validate incoming SASL `AUTHENTICATE` commands against this
list; it is purely informational.

If the list of supported mechanisms changes, services should send an updated
`ENCAP MECHLIST` with the new list. Setting the list to an empty string will
cause the sasl capability to be relayed to clients with no value.

### MLOCK

- Capability: **MLOCK**
- Source: server
- Propagation: broadcast
- Implementation: recommended
- Syntax: `MLOCK <channel TS> <channel> :<modelock>`

Sets the mode lock for a channel to the provided string. An empty string
parameter removes the mode lock. This command follows simple channel TS rules.

### MODE

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: required
- Syntax: `MODE <user> :<modes>`

Changes the user modes for the specified user. The trailing parameter may
contain additional parameters needed for the modes (i.e. snomask updates). The
source of the mode change must match the user parameter; mode changes for
other users are rejected.

----

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: required
- Syntax: `MODE <user>`

Retrieves the list of user modes for the specified user. The source must
either be the same as the target user or have the oper:auspex privilege.

----

- Capability: none
- Source: any
- Propagation: special
- Implementation: legacy
- Syntax: `MODE <channel> :<modes>`

Changes the channel modes for the specified channel. This command is
deprecated and should not be used; use `TMODE` instead as it contains a
timestamp parameter. If a bare `MODE` to modify channel modes is recieved, it
should be propagated as `TMODE` rather than `MODE` as a broadcast.

### MODLIST (E)

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `ENCAP <server mask> MODLIST :<filter>`

List all modules loaded on the server which match the specified filter. The
filter parameter may contain wildcards.

### MODLOAD (E)

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `ENCAP <server mask> MODLOAD :<module>`

Loads the specified module onto the server. The module parameter denotes the
filename on disk to load, which must be present in one of the configured
module directories. Navigation to parent directories is not allowed, and the
path must resolve to a regular file (not a symlink).

### MODRELOAD (E)

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `ENCAP <server mask> MODRELOAD :<module>`

Reloads the given module by name.

### MODRESTART (E)

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `ENCAP <server mask> MODRESTART`

Reloads all modules.

### MODUNLOAD (E)

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `ENCAP <server mask> MODUNLOAD :<module>`

Unloads the given module by name. Core modules cannot be unloaded.

### MOTD

- Capability: none
- Source: user
- Propagation: hunted server
- Implementation: recommended
- Syntax: `MOTD :<hunted>`

Displays the MOTD for the specified server.

### NICK

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: required
- Syntax: `NICK <nick> <nick TS>`

Changes the source user's nick to the provided nickname. This command follows
nick TS rules.

### NICKDELAY (E)

- Capability: **ENCAP**
- Source: server
- Propagation: broadcast
- Implementation: recommended
- Syntax: `ENCAP * NICKDELAY <duration> <nick>`

This command can only be sent by services. Prevents a nickname from being used
for duration seconds. A duration of 0 removes the delay and durations over
86400 seconds (24 hours) are treated as if they were 86400 seconds instead.

This command does not force-change someone actively using the nick to a
different nick, but will still implement the delay (preventing them from
returning to that nickname for the duration if they nick away themselves or
via `RSFNC`).

### NOTICE

- Capability: none
- Source: any
- Propagation: special
- Implementation: required
- Syntax: `NOTICE <channel> :<message>`

Sends a notice to a channel.

This command is propagated similarly to "broadcast" except it is not sent to
server links if there are no clients in the channel behind that link.

----

- Capability: none
- Source: any
- Propagation: target
- Implementation: required
- Syntax: `NOTICE <target> :<message>`

Sends a notice to a client.

----

- Capability: **CHW**
- Source: any
- Propagation: special
- Implementation: recommended
- Syntax: `NOTICE <prefix><channel> :<message>`

Sends a STATUSMSG notice to a channel. The prefix may be either "+" to send to
voiced users and ops on the channel, or "@" to send to only ops. If the source
is a user, they must have status equal to or greater than the selected prefix.
This command is not propagated to servers which do not support **CHW**.

This command is propagated similarly to "broadcast" except it is not sent to
server links if there are no clients in the channel behind that link.

----

- Capability: both **CHW** and **EOPMOD**
- Source: any
- Propagation: special
- Implementation: recommended
- Syntax: `NOTICE =<channel> :<message>`

Sends an "op moderated" message to the channel (for channel mode +z). This is
treated as an @#channel STATUSMSG; however, the source does not need to be
opped. When propagating, solanum uses the following matrix to determine what
message is sent based on the capabilities of the downstream server as well as
whether the channel is moderated (mode +m) or not:

| Capabilities           | Moderated | Message Sent                                                             |
|------------------------|-----------|--------------------------------------------------------------------------|
| **CHW** and **EOPMOD** | Either    | `:<source> NOTICE =<channel> :<message>`                                 |
| **CHW** only           | Yes       | `:<source> NOTICE @<channel> :<message>`                                 |
| **CHW** only           | No        | `:<server> NOTICE @<channel> :"<" <source> ":" <channel> "> " <message>` |
| No **CHW**             | Either    | Command is not propagated to this server                                 |

The odd fallback scenario for the +z and -m case is due to historical reasons.
The +z channel mode existed before **EOPMOD** but only worked on +m messages;
additionally, receiving servers validate that the source is able to send the
notice to the channel and will reject the command if they are not. If a server
lacks **EOPMOD** support, it means that it will only allow a @#channel message
from either an op or if the channel is +m, so the source is rewritten to be a
server to bypass this check (allowing propagating of the newer +z that allows
+bq users to speak to ops as well).

In general, implement support for both **CHW** and **EOPMOD** or neither to
avoid this insanity.

This command is propagated similarly to "broadcast" except it is not sent to
server links if there are no clients in the channel behind that link.

----

- Capability: none
- Source: any
- Propagation: single server
- Implementation: optional
- Syntax: `NOTICE <target>@<server> :<message>`

Sends a notice to a client, who must be on the specified server. Solanum
internally rejects this format for local clients, but will still propagate it
so that pseudoservers and services can accept this syntax. The target need not
actually exist on the specified server or be known to solanum for it to
propagate this message onwards.

----

- Capability: none
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `NOTICE $$<server mask> :<message>`

Sends a mass-notice to all clients on servers matching the server mask. The
mask may contain wildcards. The source must be an oper or else this command is
rejected.

----

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: optional
- Syntax: `NOTICE $#<mask> :<message>`

Sends a mass-notice to all clients whose hostnames match the given mask. The
mask may contain wildcards. The source must be an oper or else this command is
rejected.

### OPER

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: recommended
- Syntax: `OPER <name> <privset>`

Marks the source user as an oper with the provided oper name and privset. The
privset should exist locally in ircd.conf; a warning will be issued if it does
not.

### OPER (E)

- Capability: **ENCAP**
- Source: user
- Propagation: broadcast
- Implementation: legacy
- Syntax: `ENCAP * OPER <name>`

Marks the source user as an oper with the provided oper name. Because this
command lacks privset information, it should no longer be used. The
non-`ENCAP` variety should be preferred instead.

### OPERSPY (E)

- Capability: **ENCAP**
- Source: user
- Propagation: broadcast
- Implementation: recommended
- Syntax: `ENCAP * OPERSPY <command> [<args...>]`

Reports an operspy operation to opers. The args are as provided to the command
being reported, meaning this command could have an arbitrary number of
parameters (up to the limit of 15).

### OPERWALL

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: optional
- Syntax: `OPERWALL :<message>`

Sends a `WALLOPS` message to all opers with the +z user mode.

### PART

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: required
- Syntax: `PART <channel> [:<reason>]`

Removes a user from the channel, optionally with the specified reason. While
the channel parameter may contain multiple channels separated by commas to
cause the user to leave multiple channels simultaneously, treat this
functionality as legacy behavior for s2s links. When propagating a
multi-channel part sent by a client, send one `PART` per channel instead.

### PASS

- Capability: none
- Source: server
- Propagation: none
- Implementation: required
- Syntax: `PASS <password> TS <version> <sid>`

Sent during the initial connection handshake to register a server connection.

Parameters:

- password: The password in the remote server's ircd.conf
- version: The TS version number supported by this server
- sid: The server's SID

The version number must be at least 6 in order to establish a link.

### PING

- Capability: none
- Source: any
- Propagation: none
- Implementation: required
- Syntax: `PING <origin>`

Sends a ping request to a locally-connected server. The origin parameter is
ignored, but historically should be the server name (not SID) of the server
sending the `PING`. Solanum does send the SID for this parameter instead in
some cases, such as for the EOB in netjoin bursts.

----

- Capability: none
- Source: any
- Propagation: single server
- Implementation: required
- Syntax: `PING <origin> :<server>`

Sends a ping request to a remote server. The origin parameter is ignored but
historically should be the name (not SID or UID) of the sender. When
propagating, the origin parameter is replaced with the name corresponding to
the source of the command.

### PONG

- Capability: none
- Source: server
- Propagation: single server
- Implementation: required
- Syntax: `PONG <origin> :<server>`

Reply to a remote ping request. The origin parameter is the server name of the
server originating the `PONG` (i.e. the destination of the `PING`). Unlike in
`PING`, the origin parameter is propagated as-is.

### PRIVMSG

- Capability: none
- Source: any
- Propagation: special
- Implementation: required
- Syntax: `PRIVMSG <channel> :<message>`

Sends a message to a channel.

This command is propagated similarly to "broadcast" except it is not sent to
server links if there are no clients in the channel behind that link.

----

- Capability: none
- Source: any
- Propagation: target
- Implementation: required
- Syntax: `PRIVMSG <target> :<message>`

Sends a message to a client.

----

- Capability: **CHW**
- Source: any
- Propagation: special
- Implementation: recommended
- Syntax: `PRIVMSG <prefix><channel> :<message>`

Sends a STATUSMSG message to a channel. The prefix may be either "+" to send
to voiced users and ops on the channel, or "@" to send to only ops. If the
source is a user, they must have status equal to or greater than the selected
prefix. This command is not propagated to servers which do not support
**CHW**.

This command is propagated similarly to "broadcast" except it is not sent to
server links if there are no clients in the channel behind that link.

----

- Capability: both **CHW** and **EOPMOD**
- Source: any
- Propagation: special
- Implementation: recommended
- Syntax: `PRIVMSG =<channel> :<message>`

Sends an "op moderated" message to the channel (for channel mode +z). This is
treated as an @#channel STATUSMSG; however, the source does not need to be
opped. When propagating, follow the same fallback matrix as `NOTICE` for
=#channel messages. In general, implement support for both **CHW** and
**EOPMOD** or neither to avoid fallback insanity.

This command is propagated similarly to "broadcast" except it is not sent to
server links if there are no clients in the channel behind that link.

----

- Capability: none
- Source: any
- Propagation: single server
- Implementation: optional
- Syntax: `PRIVMSG <target>@<server> :<message>`

Sends a message to a client, who must be on the specified server. Solanum
internally rejects this format for local clients, but will still propagate it
so that pseudoservers and services can accept this syntax. The target need not
actually exist on the specified server or be known to solanum for it to
propagate this message onwards.

----

- Capability: none
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `PRIVMSG $$<server mask> :<message>`

Sends a mass-message to all clients on servers matching the server mask. The
mask may contain wildcards. The source must be an oper or else this command is
rejected.

----

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: optional
- Syntax: `PRIVMSG $#<mask> :<message>`

Sends a mass-message to all clients whose hostnames match the given mask. The
mask may contain wildcards. The source must be an oper or else this command is
rejected.

### PRIVS (E)

- Capability: **ENCAP**
- Source: user
- Propagation: single server
- Implementation: recommended
- Syntax: `ENCAP <server> PRIVS <target>`

Retrieves the list of privileges from oper privset (if any) as well as auth{}
block privileges for the specified target. If the target is an oper, their
oper name and privset name are produced as well. The RPL_PRIVS (270) numeric
is used for all replies.

### QUARANTINE (E)

- Capability: **ENCAP**
- Source: user
- Propagation: single server
- Implementation: optional
- Syntax: `ENCAP <server> QUARANTINE <target> :<reason>`

Places the target into quarantine (user mode +q), which prevents them from
joining or messaging most channels until they identify to services. If the
target is an oper or already identified to services, this command has no
effect. The reason is an oper-facing reason that is not shown to the target.

### REALHOST (E)

- Capability: **ENCAP**
- Source: user
- Propagation: broadcast
- Implementation: legacy
- Syntax: `ENCAP * REALHOST <host>`

Notifies the network of the source's real (not spoofed) hostname. This is sent
during connection bursts to servers that do not support **EUID**.

### REHASH (E)

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `ENCAP <server mask> REHASH [<type>]`

Directs matching servers to reload some aspect of their configuration. If the
type parameter is not provided, they should reload their configuration file.
Otherwise, type will be one of the following options:

- BANS: Reload the bans database (`BAN` command)
- DNS: Re-parse /etc/resolv.conf
- HELP: Reload help files
- MOTD: Reload the motd file
- NICKDELAY: Clear all current nick delays (`NICKDELAY` command)
- OMOTD: Reload the oper motd file
- REJECTCACHE: Clear all rejected IPs (cached bans)
- SSLD: Start new ssld helper processes
- TDLINES: Clear all temporary d-lines (`DLINE`/`ENCAP DLINE` commands)
- THROTTLES: Clear all throttled IPs (connection throttling)
- TKLINES: Clear all temporary k-lines (`KLINE`/`ENCAP KLINE` commands)
- TRESVS: Clear all temporary resvs (`RESV`/`ENCAP RESV` commands)
- TXLINES: Clear all temporary x-lines (`XLINE`/`ENCAP XLINE` commands)

### REMOVE

- Capability: **REMOVE**
- Source: any
- Propagation: broadcast
- Implementation: optional
- Syntax: `REMOVE <channel> <target> :<reason>`

Force-parts the target from the channel. When propagating to downstream
servers that do not support **REMOVE**, this command is propagated as `KICK`
instead.

### RESTART (E)

- Capability: **ENCAP**
- Source: user
- Propagation: single server
- Implementation: optional
- Syntax: `ENCAP <server> RESTART <server>`

Instructs the ircd on the specified server to restart.

### RESV

- Capability: **CLUSTER**
- Source: user
- Propagation: server mask
- Implementation: legacy
- Syntax: `RESV <server mask> <mask> :<reason>`

Sets a permanent RESV (nickname or channel name ban) on matching servers.
Nickname bans may contain wildcards, but channel name bans may not.

The reason field indicates the reason for the ban; it is not shown to
normal users, only opers.

If a RESV for the mask already exists, this command will be ignored locally
(but still propagated). When propagating this command, send `RESV` to servers
supporting **CLUSTER** and `ENCAP RESV` to other servers.

Note: This command is for setting server-specific RESVs that are not further
propagated as new servers join the network. Prefer using `BAN` instead.

Note: This command does not support temporary server-specific RESVs;
`ENCAP RESV` is required for that.

### RESV (E)

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `ENCAP <server mask> RESV <duration> <mask> 0 :<reason>`

Sets a RESV (nickname or channel name ban) on matching servers. The duration
is in minutes, with a value of 0 indicating a permanent ban. Nickname bans may
contain wildcards, but channel name bans may not. The parameter before the
reason is ignored by solanum but should be sent as a literal "0" for
historical reasons.

The reason field indicates the reason for the ban; it is not shown to
normal users, only opers.

If a RESV for the mask already exists, this command will be ignored.

Note: This command is for setting server-specific RESVs that are not further
propagated as new servers join the network. Prefer using `BAN` instead.

### RSFNC (E)

- Capability: **ENCAP**
- Source: server
- Propagation: single server
- Implementation: required
- Syntax: `ENCAP <server> RSFNC <target> <new> <new TS> <nick TS> [<override>]`

This command can only be sent by services. It forces a nickname change for the
specified target.

Parameters:

- server: The server name containing the target user
- target: The user whose nickname should be changed (UID)
- new: The nickname to change the target to; cannot start with a digit
- new TS: The user's new nickname TS after the nick change
- nick TS: The user's current nickname TS
- override: 1 to override RESVs on the new nickname, 0 to not override RESVs

If the override parameter is not specified, it defaults to 1 (override RESVs).
If the new nickname already exists on the network, it will be automatically
killed off. If the nick TS parameter does not match the user's current
nickname TS, the command will be ignored.

### QUIT

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: required
- Syntax: `QUIT :<comment>`

Sent when a user has disconnected from the network with the specified quit
comment. If the user does not provide a comment of their own, the server must
provide a default; it cannot send this command without a comment parameter.

No `QUIT` should be sent for a user that was removed as a result of `KILL`.

### SASL (E)

- Capability: **ENCAP**
- Source: server
- Propagation: single server
- Implementation: required
- Syntax: `ENCAP <server> SASL <uid> <agent> H <host> <ip> <tls>`

Provides information about a user initiating SASL to services.

Parameters:

- server: The services server containing SaslServ
- uid: The (potentially unregistered) UID of the user initiating SASL
- agent: The UID of SaslServ
- host: The (potentially spoofed) hostname of the user initiating SASL
- ip: The IP address of the user initiating SASL
- tls: "S" if the connection is secure, "P" if it is plaintext

Note: Services does not need to reply to this command.

----

- Capability: **ENCAP**
- Source: server
- Propagation: single server
- Implementation: required
- Syntax: `ENCAP <server> SASL <uid> <agent> S <mech> [<certfp>]`

Instructs services to initiate a new SASL exchange. This will be sent *after*
the "H" message containing information about the user initiating SASL.

Parameters:

- server: The services server containing SaslServ
- uid: The (potentially unregistered) UID of the user initiating SASL
- agent: The UID of SaslServ
- mech: The SASL mechanism selected by the user
- certfp: The TLS client certificate fingerprint of the user, if any

If the user did not provide a TLS client certificate, the certfp parameter
will be omitted.

Note: To ensure proper tracking of a SASL session, services must only reply
using `ENCAP SASL` rather than directly sending numerics to the provided uid.

----

- Capability: **ENCAP**
- Source: server
- Propagation: single server
- Implementation: required
- Syntax: `ENCAP <server> SASL <source> <target> C <data>`

Provides SASL payload data. For commands sent to services, the data is passed
through as-is from the user's `AUTHENTICATE` command. For commands received
from services, the data is passed through as-is to the user via an
`AUTHENTICATE` message. Consult the ircv3 SASL specification for information
about formatting and how it is split over multiple lines if necessary.

Parameters:

- server: The services server containing SaslServ (for commands sent to
  services) or the server containing the user (for commands received from
  services)
- source: The (potentially unregistered) UID of the user initiating SASL (for
  commands sent to services) or the UID of SaslServ (for commands received
  from services)
- target: The UID of SaslServ (for commands sent to services) or the UID of
  the user initiating SASL (for commands received from services)
- data: The SASL payload

Note: To ensure proper tracking of a SASL session, services must only reply
using `ENCAP SASL` rather than directly sending numerics to the provided uid.

----

- Capability: **ENCAP**
- Source: server
- Propagation: single server
- Implementation: required
- Syntax: `ENCAP <server> SASL <source> <target> D <type>`

Completes a SASL exchange.

- server: The services server containing SaslServ (for commands sent to
  services) or the server containing the user (for commands received from
  services)
- source: The (potentially unregistered) UID of the user initiating SASL (for
  commands sent to services) or the UID of SaslServ (for commands received
  from services)
- target: The UID of SaslServ (for commands sent to services) or the UID of
  the user initiating SASL (for commands received from services)
- type: "A" to indicate SASL was aborted, "F" to indicate SASL failed, or "S"
  to indicate SASL succeeded

Commands sent to services will only ever have type "A" indicating the user
aborted SASL. Commands sent from services should only ever have "F" or "S". In
rare cases (such as SaslServ disconnecting in the middle of a SASL exchange),
aborts sent by solanum may be broadcast instead (server parameter of "\*")
with an agent parameter of "\*" as well.

When received by the user's server, it will be relayed to the user using the
appropriate numeric corresponding to the type.

Note: Services does not need to reply to an abort sent by solanum.

----

- Capability: **ENCAP**
- Source: server
- Propagation: single server
- Implementation: required
- Syntax: `ENCAP <server> SASL <agent> <uid> M <mechs>`

Sent from services to give the user a list of supported SASL mechanisms.

- server: The server containing the user
- agent: The UID of SaslServ
- uid: The (potentially unregistered) UID of the user initiating SASL
- mechs: A comma-separated list of supported SASL mechanisms

This command will be relayed to the user as the RPL_SASLMECHS (908) numeric.

Note: To ensure proper tracking of a SASL session, services must only reply
using `ENCAP SASL` rather than directly sending numerics to the provided uid.

An example full SASL exchange between a user (with UID 001AAAAAA) on a server
with SID 001 and services (SID 002, with SaslServ's UID being 002BBBBBB):
```
:001 ENCAP services.example.com SASL 001AAAAAA 002BBBBBB H test.example.com 10.0.0.3 S
:001 ENCAP services.example.com SASL 001AAAAAA 002BBBBBB S PLAIN
:002 ENCAP server.example.com SASL 002BBBBBB 001AAAAAA C +
:001 ENCAP services.example.com SASL 001AAAAAA 002BBBBBB C dGVzdAB0ZXN0AGxldG1laW4=
:002 ENCAP server.example.com SASL 002BBBBBB 001AAAAAA D S
```

Example with a user specifying an invalid mechanism:
```
:001 ENCAP services.example.com SASL 001AAAAAA 002BBBBBB H test.example.com 10.0.0.3 S
:001 ENCAP services.example.com SASL 001AAAAAA 002BBBBBB S SCRAM-SHA-256
:002 ENCAP server.example.com SASL 002BBBBBB 001AAAAAA M PLAIN,EXTERNAL,SCRAM-SHA-512
:002 ENCAP server.example.com SASL 002BBBBBB 001AAAAAA D F
```

### SAVE

- Capability: **SAVE**
- Source: server
- Propagation: broadcast
- Implementation: recommended
- Syntax: `SAVE <target> <target TS>`

Changes the target's nickname to their UID. The target's nickname TS must
match the provided target TS or else this command has no effect. When
propagating to downstream servers that do not support **SAVE**, send `NICK`
instead.

After changing the target's nickname to its UID, its nickname TS should be
reset to 100. When propagating `NICK` to servers that do not support **SAVE**,
the TS parameter should be 100 as well instead of what was specified in the
incoming command.

### SERVER

- Capability: none
- Source: server
- Propagation: special
- Implementation: required
- Syntax: `SERVER <name> <hop> :<info>`

Introduces a new server to the network. The name is the server name (not SID),
hop is the number of hops between the new server and the current server, and
info is arbitrary information about the server (exposed in e.g. `LINKS`).

The info may begin with the string "(H)" followed by a space which indicates
that the server should be hidden from non-opers.

When propagating this command to the rest of the network, `SID` should be
used instead of `SERVER`. The server should increase the hop count by 1 before
sending `SID` to downstream servers.

### SETFILTER (E)

- Capability: **ENCAP**
- Source: user
- Propagation: broadcast
- Implementation: recommended
- Syntax: `ENCAP <server mask> SETFILTER <check> <action>`

Sends a control command to manipulate the hyperscan database. The check
parameter is an arbitrary string at most 20 bytes long, and must be consistent
for all operations within a single transaction.

Action is one of the following:

- NEW: Begin processing data for a new/updated database. This must be sent
  before any data is sent. If a transaction is already in progress, any data
  sent so far will be dropped
- DROP: Delete the hyperscan database (thereby disabling filtering)
- ABORT: Stop an in-progress transaction and drop all sent data, while keeping
  the current database intact
- APPLY: Finish processing data for a new/updated database and load it as the
  new active database. This is sent last after all data has been sent and
  completes the transaction. The check parameter must match the one sent with
  the NEW action

----

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: recommended
- Syntax: `ENCAP <server mask> SETFILTER <check> +<data>`

Appends data to the in-progress transaction. The check parameter must match
the one sent with the NEW action. The data must be sent as a base64-encoded
string and will be decoded before being appended to the transaction buffer.
Solanum currently has a hard limit of 10MB for the overall (decoded) database
size and will reject databases larger than this.

### SHEDDING (E)

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `ENCAP <server mask> SHEDDING <rate> :<reason>`

Enables user shedding on the specified servers. One user should be
disconnected from the server approximately every `<rate>` seconds. The rate
must be an integer greater than or equal to 5. The reason is only visible to
opers.

----

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `ENCAP <server mask> SHEDDING OFF`

Disables user shedding on the specified servers.

### SID

- Capability: none
- Source: server
- Propagation: broadcast
- Implementation: required
- Syntax: `SID <name> <hop> <sid> :<info>`

Introduces a new server to the network. The name is the server name, hop is
the number of hops between the new server and the current server, sid is that
server's SID, and info is arbitrary information about the server (exposed in
e.g. `LINKS`).

The info may begin with the string "(H)" followed by a space which indicates
that the server should be hidden from non-opers.

This command cannot be sent during the server registration; a server informs
its direct uplink about its SID via the `PASS` command instead.

### SIGNON

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: required
- Syntax: `SIGNON <nick> <user> <host> <nick TS> <account>`

This command is sent when a server receives `SVSLOGIN` for a registered user.
It indicates that the source has either signed into the given services account
or has logged out (represented by an account parameter of "0"). Additionally,
the user's nickname, username, and host may be updated by this command. This
command follows nickname TS rules with regards to nickname collisions.

### SJOIN

- Capability: none
- Source: server
- Propagation: broadcast
- Implementation: required
- Syntax: `SJOIN <channel TS> <channel> <modes> [<params...>] :<users...>`

Sent to establish a new channel to join a user to a channel with status. This
command follows channel TS rules.

Parameters:
- channel TS: the TS of the channel
- channel: channel name
- modes: simple (non-list/status) modes to set/unset on the channel; may be
  "+" if not changing modes
- params: any parameters required for modes
- users: space-separated list of channel members (UIDs) prefixed by status

The modes parameter is additive/subtractive just like the `MODE` command.
Because an arbitrary number of the modes may take parameters, the number of
parameters passed to this command can vary. For the user list, a "+" prefix is
used to denote voice, "@" for ops, and "@+" for both ops and voice. All users
must be from or behind the server this command is propagated from; it cannot
be used to force-join other users to a channel.

List modes (bans) must be propagated via `EBMASK`/`BMASK` instead of `SJOIN`.

Examples:
```
:001 SJOIN 1777418673 #example +ijlnt 5:30 100 :@001AAAAAA 001AAABBB +001AAACCC
:001 SJOIN 1777418673 #example + :001AAADDD 001AAAEEE @+001AAAFFF 001AAAGGG
```

### SNOTE (E)

- Capability: **ENCAP**
- Source: server
- Propagation: broadcast
- Implementation: recommended
- Syntax: `ENCAP * SNOTE <letter> :<message>`

Broadcasts a server notice. The letter dictates the snomask letter an oper
must have to receive the notice. An unrecognized letter will result in the
command being ignored. Consult the help/opers/snomask file for a listing of
all letters recognized by default or with an extension module in solanum.

### SQUIT

- Capability: none
- Source: any
- Propagation: broadcast
- Implementation: required
- Syntax: `SQUIT <server> :<comment>`

Disconnects a server from the network. If the server parameter matches the
current server, it should disconnect from the link the `SQUIT` arrived from
and propagate an `SQUIT` for that server instead to its downstream servers.

### STATS

- Capability: none
- Source: user
- Propagation: hunted server
- Implementation: recommended
- Syntax: `STATS <letter> <hunted>`

Requests statistics from the server on behalf of a remote user. `STATS` letter
definitions are not part of the TS6 protocol (and need not be actual letters);
some servers may support different subsets of letters than other servers. For
a listing of letters that solanum supports, consult the help/opers/stats file.
Privileges are checked by the server actually handling the request.

### SU (E)

- Capability: **ENCAP**
- Source: server
- Propagation: broadcast
- Implementation: required
- Syntax: `ENCAP * SU <target> [<account>]`

Sent by services to log the target user into or out of an account. If the
account parameter is omitted, the user will be logged out. Otherwise, they are
logged into the account name specified by the parameter.

Services should only send `ENCAP SU` for users who have completed connection
registration. If they need to change the accountname of a user who is still
connecting, they should use `ENCAP SVSLOGIN` instead. Similarly, if services
needs to change other aspects of the user (such as spoofed hostname), then
`ENCAP SVSLOGIN` is more appropriate since that can be handled as one command
rather than two (`ENCAP SU` + `CHGHOST`).

### SVINFO

- Capability: none
- Source: server
- Propagation: none
- Implementation: required
- Syntax: `SVINFO <current> <min> 0 <time>`

Sent by a server during registration. The current parameter is the TS server
protocol version that the server currently supports and the min parameter is
the minimum TS server protocol version that the server supports. For linking
to solanum, both of these parameters must be 6. The time parameter is the
current unix timestamp for the server. If the delta between this timestamp and
the local server's timestamp is too large, the link will be dropped.

Note: The third parameter is unused and should be specified as 0.
Historically, it was used to specify if a server would correct time delta
differences. It is far better to instead ensure every server on the network
has working NTP and a reasonably-synchronized clock.

### SVSLOGIN (E)

- Capability: **ENCAP**
- Source: server
- Propagation: single server
- Implementation: required
- Syntax: `ENCAP <server> SVSLOGIN <target> <nick> <user> <host> <account>`

Sent by services to modify details for a user. The target user does not need
to have completed connection registration.

Parameters:
- server: The server containing the target user
- target: UID of the user logging in/out or otherwise changing details
- nick: Nickname to set the user to, or "*" to keep their current nick
- user: Username spoof to set for the user, or "*" to keep their current user
- host: Hostname spoof to set for the user, or "*" to keep their current host
- account: Account name the user logged in as, "0" if they logged out, or "*"
  to keep their current account name (if any)

If a new nickname is set via this command, any current user of that nickname
will be killed. When `ENCAP SVSLOGIN` is received for an unregistered user, it
is not propagated further. If it is received for a registered user, it is
broadcast to other servers via the `SIGNON` message.

### TAGMSG

- Capability: **STAG**
- Source: any
- Propagation: special
- Implementation: recommended
- Syntax: `TAGMSG <channel>`

Sends tags to a channel. Solanum implements tag filtering; commands without
tags after filtering is performed will be dropped.

This command is propagated similarly to "broadcast" except it is not sent to
server links if there are no clients in the channel behind that link.

----

- Capability: **STAG**
- Source: any
- Propagation: target
- Implementation: recommended
- Syntax: `TAGMSG <target>`

Sends tags to a client. Solanum implements tag filtering; commands without
tags after filtering is performed will be dropped.

----

- Capability: both **STAG** and **CHW**
- Source: any
- Propagation: special
- Implementation: recommended
- Syntax: `TAGMSG <prefix><channel>`

Sends tags to users with a given prefix or above on a channel. The prefix may
be either "+" to send to voiced users and ops on the channel, or "@" to send
to only ops. If the source is a user, they must have status equal to or
greater than the selected prefix. This command is not propagated to servers
which do not support **CHW**. Solanum implements tag filtering; commands
without tags after filtering is performed will be dropped.

This command is propagated similarly to "broadcast" except it is not sent to
server links if there are no clients in the channel behind that link.

----

- Capability: all of **STAG**, **CHW**, and **EOPMOD**
- Source: any
- Propagation: special
- Implementation: recommended
- Syntax: `TAGMSG =<channel>`

Sends "op moderated" tag to the channel (for channel mode +z). This is treated
as an @#channel `TAGMSG`; however, the source does not need to be opped. When
propagating, follow the same fallback matrix as `NOTICE` for =#channel
messages. Solanum implements tag filtering; commands without tags after
filtering is performed will be dropped.

This command is propagated similarly to "broadcast" except it is not sent to
server links if there are no clients in the channel behind that link.

----

- Capability: none
- Source: any
- Propagation: single server
- Implementation: optional
- Syntax: `TAGMSG <target>@<server>`

Sends tags to a client, who must be on the specified server. Solanum
internally rejects this format for local clients, but will still propagate it
so that pseudoservers and services can accept this syntax. The target need not
actually exist on the specified server or be known to solanum for it to
propagate this message onwards. Solanum implements tag filtering; commands
without tags after filtering is performed will be dropped.

Note: while this syntax is supported due to `NOTICE`, `PRIVMSG`, and `TAGMSG`
all sharing the same underlying machinery, it largely has no practical use and
should be avoided. It may be removed in the future.

----

- Capability: none
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `TAGMSG $$<server mask>`

Sends a mass-tagmsg to all clients on servers matching the server mask. The
mask may contain wildcards. The source must be an oper or else this command is
rejected. Solanum implements tag filtering; commands without tags after
filtering is performed will be dropped.

Note: while this syntax is supported due to `NOTICE`, `PRIVMSG`, and `TAGMSG`
all sharing the same underlying machinery, it largely has no practical use and
should be avoided. It may be removed in the future.

----

- Capability: none
- Source: user
- Propagation: broadcast
- Implementation: optional
- Syntax: `TAGMSG $#<mask>`

Sends a mass-tagmsg to all clients whose hostnames match the given mask. The
mask may contain wildcards. The source must be an oper or else this command is
rejected. Solanum implements tag filtering; commands without tags after
filtering is performed will be dropped.

Note: while this syntax is supported due to `NOTICE`, `PRIVMSG`, and `TAGMSG`
all sharing the same underlying machinery, it largely has no practical use and
should be avoided. It may be removed in the future.

### TB

- Capability: **TB**
- Source: server
- Propagation: broadcast
- Implementation: recommended
- Syntax: `TB <channel> <topic TS> [<who>] :<topic>`

Bursts a topic change.

Parameters:
- channel: Channel name
- topic TS: Unix timestamp of when the topic was set
- who: Hostmask of the topic setter or server name if set by a server
- topic: Updated channel topic (may be an empty string)

If the provided topic TS is older than the current topic's TS, this command is
ignored. If the who parameter is omitted, the server name of the source (if
server hiding is disabled) or the server name of the local server (if server
hiding is enabled) will be used as the topic setter instead.

Unlike **ETB** this command cannot be used to change the topic setter without
also changing the topic TS and topic text. This command also cannot be used to
set the topic to an empty string; such changes will be ignored.

### TGINFO (E)

- Capability: **ENCAP**
- Source: user
- Propagation: broadcast
- Implementation: recommended
- Syntax: `ENCAP * TGINFO 0`

Used to alert other servers on the network that the source user has exceeded
their allowance of target changes, and future messages from them to new
targets will be dropped. A server notice is sent to opers informing them about
the excessive target changes (as part of processing this command, it is *not*
sent as a separate `ENCAP SNOTE` command).

The final parameter is reserved for future use as the number of target changes
remaining for the user. Right now this parameter must be "0"; any other values
will result in the command being dropped.

### TIME

- Capability: none
- Source: user
- Propagation: hunted server
- Implementation: optional
- Syntax: `TIME :<hunted>`

Retrieves the current time for the specified server. The reply is a
human-readable date and time in an RPL_TIME (391) numeric.

### TMODE

- Capability: none
- Source: any
- Propagation: broadcast
- Implementation: required
- Syntax: `TMODE <channel TS> <channel> <modes> [<params...>]`

Changes the channel modes for the specified channel. This command follows
simple channel TS rules.

### TOPIC

- Capability: none
- Source: any
- Propagation: broadcast
- Implementation: required
- Syntax: `TOPIC <channel> :<topic>`

Changes the topic for a channel. The new topic may be an empty string to clear
the topic.

### TRACE

- Capability: none
- Source: user
- Propagation: hunted server
- Implementation: recommended
- Syntax: `TRACE <target> :<hunted>`

Performs a remote trace with an explicit hunted server as the origin of the
trace. The hunted server then sends a `TRACE` command with only the target
parameter to perform the trace on the target user. The target user does not
need to be connected to the hunted server.

----

- Capability: none
- Source: user
- Propagation: target
- Implementation: recommended
- Syntax: `TRACE <target>`

Performs a trace. Each server along the path replies with information about
that hop (if server hiding is disabled) and the target server replies with
the trace information. The target parameter may be a server name to trace all
users on the server, or it may contain wildcards to trace all matching users
on the server. Non-opers may only trace themselves and get no output if the
target doesn't match their own connection (whether due to explicitly
specifying themselves, trying to trace all users, or having a wildcard mask
that includes themselves).

### UID

- Capability: none
- Source: server
- Propagation: target
- Implementation: legacy
- Syntax: `UID <nick> <hops> <nick TS> <modes> <user> <host> <ip> <uid> :<realname>`

Introduces a new user to the network. Use `EUID` instead when possible. This
command follows nickname TS rules.

Parameters:

- nick: The user's nickname
- hops: Number of hops (including the destination server, so minimum 1) it
  takes to reach the client through this link
- nick TS: The user's timestamp
- modes: User modes for the user, with leading "+" (if the user has no user
  modes, just a literal "+")
- user: The user's username
- host: The user's visible hostname
- ip: The user's IP address, or literal "0" if user has an IP spoof
- uid: The user's UID
- realname: The user's realname

### UNDLINE (E)

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `ENCAP <server mask> UNDLINE <ip>`

Removes a d-line from the specified IP or CIDR. When unbanning an IPv6 address
or CIDR, the ip cannot start with `:` (as this would otherwise be interpreted
as the beginning of the trailing parameter in IRC protocol framing). To work
around this, prefix the address with a leading 0, e.g. `0::1` to remove a ban
on localhost.

### UNKLINE

- Capability: **UNKLN**
- Source: user
- Propagation: server mask
- Implementation: legacy
- Syntax: `UNKLINE <server mask> <user> <host>`

Removes a k-line from matching servers. When unbanning an IPv6 address or
CIDR, the host cannot start with `:` (as this would otherwise be interpreted
as the beginning of the trailing parameter in IRC protocol framing). To work
around this, prefix the address with a leading 0, e.g. `0::1` to remove a ban
on localhost.

When propagating this command, send `UNKLINE` to servers supporting **UNKLN**
and `ENCAP UNKLINE` to other servers.

Note: This command is for removing server-specific k-lines that are not further
propagated as new servers join the network. Propagated k-lines set with `BAN`
must also be removed with `BAN`.

### UNKLINE (E)

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `ENCAP <server mask> UNKLINE <user> <host>`

Removes a k-line from matching servers. When unbanning an IPv6 address or
CIDR, the host cannot start with `:` (as this would otherwise be interpreted
as the beginning of the trailing parameter in IRC protocol framing). To work
around this, prefix the address with a leading 0, e.g. `0::1` to remove a ban
on localhost.

Note: This command is for removing server-specific k-lines that are not further
propagated as new servers join the network. Propagated k-lines set with `BAN`
must also be removed with `BAN`.

### UNQUARANTINE (E)

- Capability: **ENCAP**
- Source: user
- Propagation: single server
- Implementation: optional
- Syntax: `ENCAP <server> UNQUARANTINE <target>`

Removes the target from quarantine (user mode -q).

### UNRESV

- Capability: **CLUSTER**
- Source: user
- Propagation: server mask
- Implementation: legacy
- Syntax: `UNRESV <server mask> <mask>`

Removes a RESV from matching servers. When propagating this command, send
`UNRESV` to servers supporting **CLUSTER** and `ENCAP UNRESV` to other servers.

Note: This command is for removing server-specific RESVs that are not further
propagated as new servers join the network. Propagated RESVs set with `BAN`
must also be removed with `BAN`.

### UNRESV (E)

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `ENCAP <server mask> UNRESV <mask>`

Removes a RESV from matching servers.

Note: This command is for removing server-specific RESVs that are not further
propagated as new servers join the network. Propagated RESVs set with `BAN`
must also be removed with `BAN`.

### UNXLINE

- Capability: **CLUSTER**
- Source: user
- Propagation: server mask
- Implementation: legacy
- Syntax: `UNXLINE <server mask> <realname>`

Removes an x-line from matching servers. When propagating this command, send
`UNXLINE` to servers supporting **CLUSTER** and `ENCAP UNXLINE` to other
servers.

Note: This command is for removing server-specific x-lines that are not
further propagated as new servers join the network. Propagated x-lines set
with `BAN` must also be removed with `BAN`.

### UNXLINE (E)

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: legacy
- Syntax: `ENCAP <server mask> UNXLINE <realname>`

Removes an x-line from matching servers.

Note: This command is for removing server-specific x-lines that are not
further propagated as new servers join the network. Propagated x-lines set
with `BAN` must also be removed with `BAN`.

### USERS

- Capability: none
- Source: user
- Propagation: hunted server
- Implementation: recommended
- Syntax: `USERS :<hunted>`

Displays statistics about the number of local and global users.

### VERSION

- Capability: none
- Source: user
- Propagation: hunted server
- Implementation: recommended
- Syntax: `VERSION :<hunted>`

Displays server version information and the ISUPPORT list.

### WALLOPS

- Capability: none
- Source: any
- Propagation: broadcast
- Implementation: required
- Syntax: `WALLOPS :<message>`

Sends a message to all users with user mode +w.

### WHOIS

- Capability: none
- Source: user
- Propagation: hunted server
- Implementation: required
- Syntax: `WHOIS <target> :<hunted>`

Executes a remote `WHOIS` to display information about the target from the
specified server. Generally, the target is connected to the hunted server in
order to retrieve additionally local-only information such as idle time, but
this is not a requirement of the protocol.

### WHOWAS

- Capability: none
- Source: user
- Propagation: hunted server
- Implementation: recommended
- Syntax: `WHOWAS <nick> <max> :<hunted>`

Executes a remote `WHOWAS` query to display up to max entries regarding the
target nickname. Solanum caps the max parameter to 20 (treating values above
that as 20 instead) for incoming remote `WHOWAS` queries.

### XLINE

- Capability: **CLUSTER**
- Source: user
- Propagation: server mask
- Implementation: legacy
- Syntax: `XLINE <server mask> <realname> 2 :<reason>`

Sets a permanent x-line (realname ban) on matching servers. The realname
parameter must not contain spaces, but may contain the following expanded
set of wildcards and escape sequences:

- `?`: Match any single character
- `*`: Match any number of characters
- `@`: Match a letter (A-Z, a-z)
- `#`: Match a digit (0-9)
- `\s`: Space
- A backslash followed by any other character resolves to that character
  (to have a literal backslash use `\\`)

The reason field indicates the reason for the ban; it is not shown to
normal users, only opers. The parameter before the reason is ignored by
solanum but should be sent as a literal "2" for historical reasons.

If an x-line for the mask already exists, this command will be ignored locally
(but still propagated). When propagating this command, send `XLINE` to servers
supporting **CLUSTER** and `ENCAP XLINE` to other servers.

Note: This command is for setting server-specific x-lines that are not further
propagated as new servers join the network. Prefer using `BAN` instead.

Note: This command does not support temporary server-specific x-lines;
`ENCAP XLINE` is required for that.

### XLINE (E)

- Capability: **ENCAP**
- Source: user
- Propagation: server mask
- Implementation: optional
- Syntax: `ENCAP <server mask> XLINE <duration> <realname> 2 :<reason>`

Sets an x-line (realname ban) on matching servers. The duration is in minutes,
with a value of 0 indicating a permanent ban. The realname parameter must not
contain spaces, but may contain the following expanded set of wildcards and
escape sequences:

- `?`: Match any single character
- `*`: Match any number of characters
- `@`: Match a letter (A-Z, a-z)
- `#`: Match a digit (0-9)
- `\s`: Space
- A backslash followed by any other character resolves to that character
  (to have a literal backslash use `\\`)

The reason field indicates the reason for the ban; it is not shown to
normal users, only opers. The parameter before the reason is ignored by
solanum but should be sent as a literal "2" for historical reasons.

If an x-line for the the mask already exists, this command will be ignored.

Note: This command is for setting server-specific x-lines that are not further
propagated as new servers join the network. Prefer using `BAN` instead.

## Tags

When the **STAG** capability is present on both sides of the link, the
following table lists which tags are accepted on incoming messages received
from remote servers, as well as the module needed to enable support for that
tag. Unrecognized tags are stripped and not propagated further.

| Tag                    | Module               | Implementation |
|------------------------|----------------------|----------------|
| batch                  | m_batch              | required       |
| msgid                  | tag_message_id       | recommended    |
| time                   | cap_server_time      | recommended    |
| solanum.chat/response  | cap_labeled_response | required       |
| +channel-context       | tag_channel_context  | optional       |
| +reply                 | tag_reply            | optional       |
| +typing                | tag_typing           | optional       |

### batch

Messages with a batch tag that do not correspond to an open `BATCH` between
the linked servers will be silently dropped.

### msgid

This tag may be attached to `PRIVMSG`, `NOTICE`, and `TAGMSG` commands and
should be forwarded as-is between servers. In order to ensure compatibility
with the +reply client-only tag, generated msgids should adhere to the string
`1<ts><ms><ctr><uid>[<chan>]` where:

- ts is the current seconds since epoch (10 characters)
- ms is the current milliseconds value for current time (3 characters)
- ctr is a counter, reinitialized to a random value each second (6 characters)
- uid is the sender's UID (9 characters)
- chan is a base64-encoded channel name, only present for channel messages
  (arbitrary character count)

If a particular value is below the number of characters specified, it must be
zero-padded to the left to meet the character count. Other msgid formats will
still be relayed faithfully to clients, but they will not be able to create a
+reply to them from solanum servers.

### solanum.chat/response

This tag may be attached to any message and is used for propagation of labeled
responses between servers (for labeled-response). The tag value must be
present and must conform to the syntax `<uid>,<batchid>,<mask>` where uid is
the UID of the client who is requesting the labeled-response, batchid is the
id of the labeled-response `BATCH` on the originating server that was sent to
the client, and mask is a server mask (potentially containing wildcards) that
indicates which servers `ENCAP ACK` is expected from. The tag and its value
must be passed along as-is in any messages generated as a response to the
incoming message.

When receiving an incoming `ENCAP` message bearing this tag, the server must
respond with `ENCAP ACK` if the mask matches the current server's name, even
if it otherwise has no handler for the encapsulated command. If the server has
a handler for the encapsulated command, `ENCAP ACK` must be sent only after
all regular replies have been sent (as it signals replies have completed).

### time

The server-time of when the message was sent. Receiving servers will not
synthesize their own server-time when **STAG** is present on both sides of the
link, so the originating server should attach this tag to all messages it
sends.

### +channel-context

The +channel-context client-only tag is forwarded as-is between servers.

### +reply

The +reply client-only tag is forwarded as-is between servers.

### +typing

The +typing client-only tag is forwarded as-is between servers.

## Batches

In addition to requiring **STAG** due to the batch tag needing to be present
on messages inside of the batch, each batch type has its own capability that
must be present on both sides of the link before batches of that type can be
sent across the link. Solanum delays processing batches until the complete
batch has been received. Servers can have any number of in-flight batches, can
nest batches, and can interleave batched and non-batched messages. Open S2S
batches do not time out automatically and will remain open until the remote
server finishes it or disconnects.

Solanum does not currently support any S2S batch types.
