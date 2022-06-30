Oper privileges
===============

These are specified in privset{}.

oper:admin, server administrator
--------------------------------

Various privileges intended for server administrators. Among other
things, this automatically sets umode +a and allows loading modules.

oper:die, die and restart
-------------------------

This grants permission to use ``DIE`` and ``RESTART``, shutting down or
restarting the server.

oper:global\_kill, global kill
------------------------------

Allows using ``KILL`` on users on any server.

oper:hidden, hide from /stats p
-------------------------------

This privilege currently does nothing, but was designed to hide bots
from /stats p so users will not message them for help.

oper:hidden\_admin, hidden administrator
----------------------------------------

This grants everything granted to the oper:admin privilege, except the
ability to set umode +a. If both oper:admin and oper:hidden\_admin are
possessed, umode +a can still not be used.

oper:kline, kline and dline
---------------------------

Allows using ``KLINE`` and ``DLINE``, to ban users by user@host mask or IP
address.

oper:local\_kill, kill local users
----------------------------------

This grants permission to use ``KILL`` on users on the same server,
disconnecting them from the network.

oper:mass\_notice, global notices and wallops
---------------------------------------------

Allows using server name ($$mask) and hostname ($#mask) masks in ``NOTICE``
and ``PRIVMSG`` to send a message to all matching users, and allows using
the ``WALLOPS`` command to send a message to all users with umode +w set.

oper:operwall, send/receive operwall
------------------------------------

Allows using the ``OPERWALL`` command and umode +z to send and receive
operwalls.

oper:rehash, rehash
-------------------

Allows using the ``REHASH`` command, to rehash various configuration files
or clear certain lists.

oper:remoteban, set remote bans
-------------------------------

This grants the ability to use the ON argument on ``DLINE``/``KLINE``/``XLINE``/``RESV``
and ``UNDLINE``/``UNKLINE``/``UNXLINE``/``UNRESV`` to set and unset bans on other
servers, and the server argument on ``REHASH``. This is only allowed if the
oper may perform the action locally, and if the remote server has a
shared{} block.

.. note:: If a cluster{} block is present, bans are sent remotely even
          if the oper does not have oper:remoteban privilege.

oper:resv, channel control
--------------------------

This allows using /resv, /unresv and changing the channel modes +L and
+P.

oper:routing, remote routing
----------------------------

This allows using the third argument of the ``CONNECT`` command, to instruct
another server to connect somewhere, and using ``SQUIT`` with an argument
that is not locally connected. (In both cases all opers with +w set will
be notified.)

oper:spy_channel, use operspy on channels
-----------------------------------------

This allows using ``/mode !#channel``, ``/whois !nick``, ``/who !#channel``,
``/chantrace !#channel``, ``/topic !#channel``, to see through secret channels,
see details of channels you are not int, etc.

All operspy usage is broadcasted to opers with snomask ``+Z`` set (on the
entire network) and optionally logged. If you grant this to anyone, it
is a good idea to establish concrete policies describing what it is to
be used for, and what not.

oper:spy_user, use operspy on user info
---------------------------------------

This allows using ``/masktrace !nick!user@host :gecos``, ``/who !mask``, ``/scan
umodes +modes-modes global list``, to see user info in bulk.

If ``operspy_dont_care_user_info`` is enabled, these do not generate ``+Z`` notices
or logs.

oper:unkline, unkline and undline
---------------------------------

Allows using ``UNKLINE`` and ``UNDLINE``.

oper:xline, xline and unxline
-----------------------------

Allows using ``XLINE`` and ``UNXLINE``, to ban/unban users by realname.

snomask:nick\_changes, see nick changes
---------------------------------------

Allows using snomask ``+n`` to see local client nick changes. This is
designed for monitor bots.
