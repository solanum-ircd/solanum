Server config file format
=========================

General format
~~~~~~~~~~~~~~

The config file consists of a series of BIND-style blocks. Each block
consists of a series of values inside it which pertain to configuration
settings that apply to the given block.

Several values take lists of values and have defaults preset inside
them. Prefix a keyword with a tilde (``~``) to override the default and
disable it.

A line may also be a .include directive, which is of the form::

  .include "file"

and causes file to be read in at that point, before the rest of
the current file is processed. Relative paths are first tried relative
to ``PREFIX`` and then relative to ``ETCPATH`` (normally ``PREFIX``/etc).

Anything from a ``#`` to the end of a line is a comment. Blank lines are
ignored. C-style comments are also supported.

Specific blocks and directives
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Not all configuration blocks and directives are listed here, only the
most common ones. More blocks and directives will be documented in later
revisions of this manual.

loadmodule directive
--------------------

::

   loadmodule "text";

Loads a module into the IRCd. In charybdis 1.1, most modules are
automatically loaded in. In future versions, it is intended to remove
this behaviour as to allow for easy customization of the IRCd's
featureset.

serverinfo {} block
-------------------

::

   serverinfo {
        name = "text";
        sid = "text";
        description = "text";
        network_name = "text";
        network_desc = "text";
        hub = boolean;
        vhost = "text";
        vhost6 = "text";
   };

The serverinfo {} block defines the core operational parameters of the
IRC server.

**serverinfo {} variables**

name
    The name of the IRC server that you are configuring. This must
    contain at least one dot. It is not necessarily equal to any DNS
    name. This must be unique on the IRC network.

sid
    A unique ID which describes the server. This consists of one digit
    and two characters which can be digits or letters.

description
    A user-defined field of text which describes the IRC server. This
    information is used in ``/links`` and ``/whois`` requests. Geographical
    location information could be a useful use of this field, but most
    administrators put a witty saying inside it instead.

network\_name
    The name of the IRC network that this server will be a member of.
    This is used in the welcome message and ``NETWORK=`` in 005.

hub
    A boolean which defines whether or not this IRC server will be
    serving as a hub, i.e. have multiple servers connected to it.

vhost
    An optional text field which defines an IPv4 address from which
    to connect outward to other IRC servers.

vhost6
    An optional text field which defines an IPv6 address from which
    to connect outward to other IRC servers.

admin {} block
--------------

::

   admin {
       name = "text";
	   description = "text";
	   email = "text";
   };

This block provides the information which is returned by the ``ADMIN``
command.

name
    The name of the administrator running this service.

description
    The description of the administrator's position in the network.

email
    A point of contact for the administrator, usually an e-mail address.

class {} block
--------------

::

    class "name" {
            ping_time = duration;
            number_per_ident = number;
            number_per_ip = number;
            number_per_ip_global = number;
            cidr_ipv4_bitlen = number;
            cidr_ipv6_bitlen = number;
            number_per_cidr = number;
            max_number = number;
            sendq = size;
    };
    
    class "name" {
            ping_time = duration;
            connectfreq = duration;
            max_number = number;
            sendq = size;
    };    
   
Class blocks define classes of connections for later use. The class name
is used to connect them to other blocks in the config file (auth{} and
connect{}). They must be defined before they are used.

Classes are used both for client and server connections, but most
variables are different.

**class {} variables: client classes**

ping\_time
    The amount of time between checking pings for clients, e.g.: 2
    minutes

number\_per\_ident
    The amount of clients which may be connected from a single identd
    username on a per-IP basis, globally. Unidented clients all count as
    the same username.

number\_per\_ip
    The amount of clients which may be connected from a single IP
    address.

number\_per\_ip\_global
    The amount of clients which may be connected globally from a single
    IP address.

cidr\_ipv4\_bitlen
    The netblock length to use with CIDR-based client limiting for IPv4
    users in this class (between 0 and 32).

cidr\_ipv6\_bitlen
    The netblock length to use with CIDR-based client limiting for IPv6
    users in this class (between 0 and 128).

number\_per\_cidr
    The amount of clients which may be connected from a single netblock.

    If this needs to differ between IPv4 and IPv6, make different
    classes for IPv4 and IPv6 users.

max\_number
    The maximum amount of clients which may use this class at any given
    time.

sendq
    The maximum size of the queue of data to be sent to a client before
    it is dropped.

**class {} variables: server classes**

ping\_time
    The amount of time between checking pings for servers, e.g.: 2
    minutes

connectfreq
    The amount of time between autoconnects. This must at least be one
    minute, as autoconnects are evaluated with that granularity.

max\_number
    The amount of servers to autoconnect to in this class. More
    precisely, no autoconnects are done if the number of servers in this
    class is greater than or equal max\_number

sendq
    The maximum size of the queue of data to be sent to a server before
    it is dropped.

auth {} block
-------------

::

    auth {
    	user = "hostmask";
    	password = "text";
    	spoof = "text";
    	flags = list;
    	class = "text";
    };

auth {} blocks allow client connections to the server, and set various
properties concerning those connections.

Auth blocks are evaluated from top to bottom in priority, so put special
blocks first.

auth {} variables
~~~~~~~~~~~~~~~~~

user
    A hostmask (``user@host``) that the auth {} block applies to. It is
    matched against the hostname and IP address (using :: shortening for
    IPv6 and prepending a 0 if it starts with a colon) and can also use
    CIDR masks. You can have multiple user entries.

password
    An optional password to use for authenticating into this auth{}
    block. If the password is wrong the user will not be able to connect
    (will not fall back on another auth{} block).

spoof
    An optional fake hostname (or ``user@host``) to apply to users
    authenticated to this auth{} block. In ``STATS i`` and ``TESTLINE``, an
    equals sign (=) appears before the ``user@host`` and the spoof is shown.

flags
    A list of flags to apply to this ``auth{}`` block. They are listed
    below. Some of the flags appear as a special character,
    parenthesized in the list, before the ``user@host`` in ``STATS i`` and
    ``TESTLINE``.

class
    A name of a class to put users matching this auth{} block into.

umodes
    Additional umodes to apply to the default_umodes upon connect.

auth {} flags
~~~~~~~~~~~~~

encrypted
    The password used has been encrypted.

spoof\_notice
    Causes the IRCd to send out a server notice when activating a spoof
    provided by this auth{} block.

exceed\_limit (>)
    Users in this auth{} block can exceed class-wide limitations.

dnsbl\_exempt ($)
    Users in this auth{} block are exempted from DNS blacklist checks.
    However, they will still be warned if they are listed.

kline\_exempt (^)
    Users in this auth{} block are exempted from DNS blacklists, k:lines
    and x:lines.

spambot\_exempt
    Users in this auth{} block are exempted from spambot checks.

shide\_exempt
    Users in this auth{} block are exempted from some serverhiding
    effects.

jupe\_exempt
    Users in this auth{} block do not trigger an alarm when joining
    juped channels.

resv\_exempt
    Users in this auth{} block may use reserved nicknames and channels.

    .. note:: The initial nickname may still not be reserved.
          
flood\_exempt (\|) Users in this auth{} block may send arbitrary
    amounts of commands per time unit to the server. This does not
    exempt them from any other flood limits. You should use this
    setting with caution.

no\_tilde (-)
    Users in this auth{} block will not have a tilde added to their
    username if they do not run identd.

need\_ident (+)
    Users in this auth{} block must have identd, otherwise they will be
    rejected.

need\_ssl
    Users in this auth{} block must be connected via SSL/TLS, otherwise
    they will be rejected.

need\_sasl
    Users in this auth{} block must identify via SASL, otherwise they
    will be rejected.

exempt {} block
---------------

::

    exempt {
    	ip = "ip";
    };

An exempt block specifies IP addresses which are exempt from ``D:lines`` and
throttling. Multiple addresses can be specified in one block. Clients
coming from these addresses can still be ``K/G/X:lined`` or banned by a DNS
blacklist unless they also have appropriate flags in their auth{} block.

**exempt {} variables**

ip
    The IP address or CIDR range to exempt.

privset {} block
----------------

::
   
    privset {
    	extends = "name";
    	privs = list;
    };

A privset (privilege set) block specifies a set of operator privileges.

**privset {} variables**

extends
    An optional privset to inherit. The new privset will have all
    privileges that the given privset has.

privs
    Privileges to grant to this privset. These are described in the
    operator privileges section.

operator {} block
-----------------

::

   operator "name" {
    	user = "hostmask";
    	password = "text";
    	rsa_public_key_file = "text";
    	umodes = list;
    	snomask = "text";
    	flags = list;
   };

Operator blocks define who may use the ``OPER`` command to gain extended
privileges.

**operator {} variables**

user
    A hostmask that users trying to use this operator {} block must
    match. This is checked against the original host and IP address;
    CIDR is also supported. So auth {} spoofs work in operator {}
    blocks; the real host behind them is not checked. Other kind of
    spoofs do not work in operator {} blocks; the real host behind them
    is checked.

    Note that this is different from charybdis 1.x where all kinds of
    spoofs worked in operator {} blocks.

password
    A password used with the ``OPER`` command to use this operator {} block.
    Passwords are encrypted by default, but may be unencrypted if
    ~encrypted is present in the flags list.

rsa\_public\_key\_file
    An optional path to a RSA public key file associated with the
    operator {} block. This information is used by the ``CHALLENGE``
    command, which is an alternative authentication scheme to the
    traditional ``OPER`` command.

umodes
    A list of usermodes to apply to successfully opered clients.

snomask
    An snomask to apply to successfully opered clients.

privset
    The privilege set granted to successfully opered clients. This must
    be defined before this operator{} block.

flags
    A list of flags to apply to this operator{} block. They are listed
    below.

**operator {} flags**

encrypted
    The password used has been encrypted. This is enabled by default,
    use ~encrypted to disable it.

need\_ssl
    Restricts use of this operator{} block to SSL/TLS connections only.

connect {} block
----------------

::
       
    connect "name" {
    	host = "text";
    	send_password = "text";
    	accept_password = "text";
    	port = number;
    	hub_mask = "mask";
    	leaf_mask = "mask";
    	class = "text";
    	flags = list;
    	aftype = protocol;
    };

Connect blocks define what servers may connect or be connected to.

**connect {} variables**

host
    The hostname or IP to connect to.

    .. note:: Furthermore, if a hostname is used, it must have an
              ``A`` or ``AAAA`` record (no ``CNAME``) and it must be
              the primary hostname for inbound connections to work.

send\_password
    The password to send to the other server.

accept\_password
    The password that should be accepted from the other server.

port
    The port on the other server to connect to.

hub\_mask
    An optional domain mask of servers allowed to be introduced by this
    link. Usually, "\*" is fine. Multiple hub\_masks may be specified,
    and any of them may be introduced. Violation of hub\_mask and
    leaf\_mask restrictions will cause the local link to be closed.

leaf\_mask
    An optional domain mask of servers not allowed to be introduced by
    this link. Multiple leaf\_masks may be specified, and none of them
    may be introduced. leaf\_mask has priority over hub\_mask.

class
    The name of the class this server should be placed into.

flags
    A list of flags concerning the connect block. They are listed below.

aftype
    The protocol that should be used to connect with, either ipv4 or
    ipv6. This defaults to neither, allowing connection using either
    address family.

**connect {} flags**

encrypted
    The value for accept\_password has been encrypted.

autoconn
    The server should automatically try to connect to the server defined
    in this connect {} block if it's not connected already and
    max\_number in the class is not reached yet.

compressed
    Ziplinks should be used with this server connection. This compresses
    traffic using zlib, saving some bandwidth and speeding up netbursts.

    If you have trouble setting up a link, you should turn this off as
    it often hides error messages.

topicburst
    Topics should be bursted to this server.

    This is enabled by default.

listen {} block
---------------

::

   listen {
    	host = "text";
    	port = number;
   };

A listen block specifies what ports a server should listen on.

**listen {} variables**

host
    An optional host to bind to. Otherwise, the ircd will listen on all
    available hosts.

port
    A port to listen on. You can specify multiple ports via commas, and
    define a range by seperating the start and end ports with two dots
    (..).

modules {} block
----------------

::
   
    modules {
    	path = "text";
    	module = text;
    };

The modules block specifies information for loadable modules.

**modules {} variables**

path
    Specifies a path to search for loadable modules.

module
    Specifies a module to load, similar to loadmodule.

general {} block
----------------

::

    modules {
    	values
    };

The general block specifies a variety of options, many of which were in
``config.h`` in older daemons. The options are documented in
``reference.conf``.

channel {} block
----------------

::

    modules {
    	values
    };

The channel block specifies a variety of channel-related options, many
of which were in ``config.h`` in older daemons. The options are
documented in ``reference.conf``.

serverhide {} block
-------------------

::

    modules {
    	values
    };

The serverhide block specifies options related to server hiding. The
options are documented in ``reference.conf``.

blacklist {} block
------------------

::

    blacklist {
    	host = "text";
    	reject_reason = "text";
    };

The blacklist block specifies DNS blacklists to check. Listed clients
will not be allowed to connect. IPv6 clients are not checked against
these.

Multiple blacklists can be specified, in pairs with first host then
reject\_reason.

**blacklist {} variables**

host
    The DNSBL to use.

reject\_reason
    The reason to send to listed clients when disconnecting them.

alias {} block
--------------

::

    alias "name" {
    	target = "text";
    };

Alias blocks allow the definition of custom commands. These commands
send ``PRIVMSG`` to the given target. A real command takes precedence above
an alias.

**alias {} variables**

target
    The target nick (must be a network service (umode ``+S``)) or
    user@server. In the latter case, the server cannot be this server,
    only opers can use user starting with "opers" reliably and the user
    is interpreted on the target server only so you may need to use
    nick@server instead).

cluster {} block
----------------

::
   
    cluster {
    	name = "text";
    	flags = list;
    };
    
The cluster block specifies servers we propagate things to
automatically. This does not allow them to set bans, you need a separate
shared{} block for that.

Having overlapping cluster{} items will cause the command to be executed
twice on the target servers. This is particularly undesirable for ban
removals.

The letters in parentheses denote the flags in ``/stats`` U.

**cluster {} variables**

name
    The server name to share with, this may contain wildcards and may be
    stacked.

flags
    The list of what to share, all the name lines above this (up to
    another flags entry) will receive these flags. They are listed
    below.

**cluster {} flags**

kline (K)
    Permanent ``K:lines``

tkline (k)
    Temporary ``K:lines``

unkline (U)
    ``K:line`` removals

xline (X)
    Permanent ``X:lines``

txline (x)
    Temporary ``X:lines``

unxline (Y)
    ``X:line`` removals

resv (Q)
    Permanently reserved nicks/channels

tresv (q)
    Temporarily reserved nicks/channels

unresv (R)
    ``RESV`` removals

locops (L)
    ``LOCOPS`` messages (sharing this with \* makes ``LOCOPS`` rather similar to
    ``OPERWALL`` which is not useful)

all
    All of the above

shared {} block
---------------

::
   
    shared {
    	oper = "user@host", "server";
    	flags = list;
    };

The shared block specifies opers allowed to perform certain actions on
our server remotely. These are ordered top down. The first one matching
will determine the oper's access. If access is denied, the command will
be silently ignored.

The letters in parentheses denote the flags in ``/stats U``.

**shared {} variables**

oper
    The user@host the oper must have, and the server they must be on.
    This may contain wildcards.

flags
    The list of what to allow, all the oper lines above this (up to
    another flags entry) will receive these flags. They are listed
    below.

    .. note:: While they have the same names, the flags have subtly
              different meanings from those in the cluster{} block.

**shared {} flags**

kline (K)
    Permanent and temporary ``K:lines``

tkline (k)
    Temporary ``K:lines``

unkline (U)
    ``K:line`` removals

xline (X)
    Permanent and temporary ``X:lines``

txline (x)
    Temporary ``X:lines``

unxline (Y)
    ``X:line`` removals

resv (Q)
    Permanently and temporarily reserved nicks/channels

tresv (q)
    Temporarily reserved nicks/channels

unresv (R)
    ``RESV`` removals

all
    All of the above; this does not include locops, rehash, dline,
    tdline or undline.

locops (L)
    ``LOCOPS`` messages (accepting this from \* makes ``LOCOPS`` rather similar
    to ``OPERWALL`` which is not useful); unlike the other flags, this can
    only be accepted from \*@\* although it can be restricted based on
    source server.

rehash (H)
    ``REHASH`` commands; all options can be used

dline (D)
    Permanent and temporary ``D:lines``

tdline (d)
    Temporary ``D:lines``

undline (E)
    ``D:line`` removals

none
    Allow nothing to be done

service {} block
----------------

::

    service {
    	name = "text";
    };

The service block specifies privileged servers (services). These servers
have extra privileges such as setting login names on users and
introducing clients with umode ``+S`` (unkickable, hide channels, etc). This
does not allow them to set bans, you need a separate shared{} block for
that.

Do not place normal servers here.

Multiple names may be specified but there may be only one service{}
block.

**service {} variables**

name
    The server name to grant special privileges. This may not contain
    wildcards.

Hostname resolution (DNS)
~~~~~~~~~~~~~~~~~~~~~~~~~

Charybdis uses solely DNS for all hostname/address lookups (no
``/etc/hosts`` or anything else). The DNS servers are taken from
``/etc/resolv.conf``. If this file does not exist or no valid IP
addresses are listed in it, the local host (``127.0.0.1``) is used. (Note
that the latter part did not work in older versions of Charybdis.)

IPv4 as well as IPv6 DNS servers are supported, but it is not possible
to use both IPv4 and IPv6 in ``/etc/resolv.conf``.

For both security and performance reasons, it is recommended that a
caching nameserver such as BIND be run on the same machine as Charybdis
and that ``/etc/resolv.conf`` only list ``127.0.0.1``.
