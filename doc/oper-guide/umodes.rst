User modes
==========

``+a``, server administrator
----------------------------

This vanity usermode is used to denote a server administrator in WHOIS
output. All local “admin” privileges are independent of it, though
services packages may grant extra privileges to ``+a`` users.

``+D``, deaf
------------

.. note:: This is a user umode, which anybody can set. It is not
          specific to operators.

Users with the ``+D`` umode set will not receive messages sent to channels.
Joins, parts, topic changes, mode changes, etc are received as normal,
as are private messages.

Support of this umode is indicated by the ``DEAF`` token in ``RPL_ISUPPORT``
(005); the parameter indicates the letter of the umode. Note that
several common IRCD implementations have an umode like this (typically
``+d``) but do not have the token in 005.

``+g``, Caller ID
-----------------

.. note:: This is a user umode, which anybody can set. It is not
    specific to operators.

Users with the ``+g`` umode set will only receive private messages
from users on a session-defined whitelist, defined by the ``/accept``
command. If a user who is not on the whitelist attempts to send a
private message, the target user will receive a rate-limited notice
saying that the user wishes to speak to them.

Network operators are not affected by the callerid whitelist system in
the event that they need to speak to users who have it enabled.

Support of this umode is indicated by the ``CALLERID`` token in
``RPL_ISUPPORT`` (005); the optional parameter indicates the letter of
the umode, otherwise ``+g``.

``+i``, invisible
-----------------

.. note:: This is a user umode, which anybody can set. It is not
          specific to operators.

Invisible users do not show up in ``WHO`` and ``NAMES`` unless you can see them.

``+o``, operator
----------------

This indicates global operator status.

``+Q``, disable forwarding
--------------------------

.. note:: This is a user umode, which anybody can set. It is not
          specific to operators.

This umode prevents you from being affected by channel forwarding. If
enabled on a channel, channel forwarding sends you to another channel if
you could not join. See channel mode ``+f`` for more information.

``+R``, reject messages from unauthenticated users
--------------------------------------------------

.. note:: This is a user umode, which anybody can set. It is not
          specific to operators.

If a user has the ``+R`` umode set, then any users who are not authenticated
will receive an error message if they attempt to send a private message
or notice to the ``+R`` user.

Opers and accepted users (like in ``+g``) are exempt. Unlike ``+g``, the target
user is not notified of failed messages.

``+s``, receive server notices
------------------------------

This umode allows an oper to receive server notices. The requested types
of server notices are specified as a parameter (“snomask”) to this
umode.

``+S``, network service
-----------------------

.. note:: This umode can only be set by servers named in a service{}
          block.

This umode grants various features useful for services. For example,
clients with this umode cannot be kicked or deopped on channels, can
send to any channel, do not show channels in ``WHOIS``, can be the target of
services aliases and do not appear in ``/stats p``. No server notices are
sent for hostname changes by services clients; server notices about
kills are sent to snomask ``+k`` instead of ``+s``.

The exact effects of this umode are variable; no user or oper on an
actual charybdis server can set it.

``+w``, receive wallops
-----------------------

.. note:: This is a user umode, which anybody can set. It is not
          specific to operators.

Users with the ``+w`` umode set will receive ``WALLOPS`` messages sent by opers.
Opers with ``+w`` additionally receive ``WALLOPS`` sent by servers (e.g. remote
``CONNECT``, remote ``SQUIT``, various severe misconfigurations, many services
packages).

``+z``, receive operwall
------------------------

``OPERWALL`` differs from ``WALLOPS`` in that the ability to receive such
messages is restricted. Opers with ``+z`` set will receive ``OPERWALL``
messages.

``+Z``, SSL user
----------------

This umode is set on clients connected via SSL/TLS. It cannot be set or
unset after initial connection.

Snomask usage
=============

Usage is as follows::

  MODE nick +s +/-flags

To set snomasks.

::

   MODE nick -s

To clear all snomasks.

Umode ``+s`` will be set if at least one snomask is set.

Umode ``+s`` is oper only by default, but even if you allow nonopers to set
it, they will not get any server notices.

Meanings of server notice masks
===============================

``+b``, bot warnings
--------------------

Opers with the ``+b`` snomask set will receive warning messages from the
server when potential flooders and spambots are detected.

``+c``, client connections
--------------------------

Opers who have the ``+c`` snomask set will receive server notices when
clients attach to the local server.

``+C``, extended client connection notices
------------------------------------------

Opers who have the ``+C`` snomask set will receive server notices when
clients attach to the local server. Unlike the ``+c`` snomask, the
information is displayed in a format intended to be parsed by scripts,
and includes the two unused fields of the ``USER`` command.

``+d``, debug
-------------

The ``+d`` snomask provides opers extra information which may be of interest
to debuggers. It will also cause the user to receive server notices if
certain assertions fail inside the server. Its precise meaning is
variable. Do not depend on the effects of this snomask as they can and
will change without notice in later revisions.

``+f``, full warning
--------------------

Opers with the ``+f`` snomask set will receive notices when a user
connection is denied because a connection limit is exceeded (one of the
limits in a class{} block, or the total per-server limit settable with
``/quote set max``).

``+F``, far client connection notices
-------------------------------------

.. note:: This snomask is only available if the ``sno_farconnect.so``
          extension is loaded.

Opers with ``+F`` receive server notices when clients connect or disconnect
on other servers. The notices have the same format as those from the ``+c``
snomask, except that the class is ? and the source server of the notice
is the server the user is/was on.

No notices are generated for netsplits and netjoins. Hence, these
notices cannot be used to keep track of all clients on the network.

There is no far equivalent of the ``+C`` snomask.

``+k``, server kill notices
---------------------------

Opers with the ``+k`` snomask set will receive server notices when services
kill users and when other servers kill and save (forced nick change to
UID) users. Kills and saves by this server are on ``+d`` or ``+s``.

``+n``, nick change notices
---------------------------

An oper with ``+n`` set will receive a server notice every time a local user
changes their nick, giving the old and new nicks. This is mostly useful
for bots that track all users on a single server.

``+r``, notices on name rejections
----------------------------------

Opers with this snomask set will receive a server notice when somebody
tries to use an invalid username, or if a dumb HTTP proxy tries to
connect.

``+s``, generic server notices
------------------------------

This snomask allows an oper to receive generic server notices. This
includes kills from opers (except services).

``+u``, unauthorized connections
--------------------------------

This snomask allows an oper to see when users try to connect who do not
have an available auth{} block.

``+W``, whois notifications
---------------------------

.. note:: This snomask is only available if the ``sno_whois.so``
          extension is loaded.

Opers with ``+W`` receive notices when a ``WHOIS`` is executed on them on their
server (showing idle time).

``+x``, extra routing notices
-----------------------------

Opers who have the ``+x`` snomask set will get notices about servers
connecting and disconnecting on the whole network. This includes all
servers connected behind the affected link. This can get rather noisy
but is useful for keeping track of all linked servers.

``+y``, spy
-----------

Opers with ``+y`` receive notices when users try to join ``RESV``'ed (“juped”)
channels. Additionally, if certain extension modules are loaded, they
will receive notices when special commands are used.

``+Z``, operspy notices
-----------------------

Opers with ``+Z`` receive notices whenever an oper anywhere on the network
uses operspy.

This snomask can be configured to be only effective for admins.
