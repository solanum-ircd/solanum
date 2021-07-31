Connecting servers
==================

Servers can be connected together to improve redundancy, distribute bandwidth,
lower latency, and connect network services.

This document is an introduction to connecting servers. It assumes you are
already somewhat familiar with Solanum's configuration (if not, read
:file:`ircd.conf.example`, and set up your own server by editing it
and running Solanum).

Solanum uses the TS6 protocol, and can only be connected with other servers
using this protocol. We recommend you only connect Solanum with other Solanum
instances.

Unlike some other IRCd implementations, all connections are reciprocal in
Solanum, which means a single configuration block is used for both incoming
and outgoing connections.
Additionally, the same ports are used for server and client connections.

Creating servers
----------------

If you already have a server running, copy its configuration to a new machine,
and edit ``serverinfo`` for the new server. In particular, you must change the
``name`` and ``sid``, but keep the same ``network_name``.
We recommend you keep both configurations in sync using some external
configuration management systems, so server configurations do not drift apart
over time, as you change them.

For each of the two servers, you must create a ``connect`` block to represent
the connection with the other server. For example, if you have servers A and B
respectively at a.example.org and b.example.org, use respectively::

   serverinfo {
           name = "a.example.org";
           // ...
   };

   connect "b.example.org" {
           host = "203.0.113.2";
           port = 6666;

           send_password = "password";
           accept_password = "anotherpassword";

           flags = topicburst, autoconn;

           class = "server";
   };

and::

   serverinfo {
           name = "b.example.org";
           // ...
   };

   connect "a.example.org" {
           host = "203.0.113.1";
           port = 6666;

           send_password = "anotherpassword";
           accept_password = "password";

           flags = topicburst, autoconn;

           class = "server";
   };

Note the reversed passwords.

The ports should be any of the ports defined in a ``listen {}`` block of the
other server.

The ``autoconn`` flag indicates a server should automatically connect using
this ``connect {}`` block. At least one of the two servers should have it,
or the servers won't try to connect.

If you are connecting servers over an unencrypted link, you should use SSL/TLS
for the connection; see :file:`reference.conf`.


Connecting services
-------------------

In addition to regular servers, you can also connect service packages such
as atheme-services.

These services typically do not accept incoming connections, and connect to
one of the existing servers of the network.

To allow connections from such a service server, you should create
a new ``connect {}`` block for this package, on the server the services
will connect to::

   connect "services.example.org" {
           host = "localhost";
           port = 6666;

           send_password = "password";
           accept_password = "anotherpassword";

           flags = topicburst;  // No autoconn, services don't accept incoming connections

           class = "server";
   };

And create the appropriate config in your services' configuration so that
they connect to your server on the configured port, and from the configured
hostname.

For example, with atheme::

   loadmodule "modules/protocol/charybdis";

   uplink "a.example.org" {
           host = "localhost";
           port = 6666;
           send_password = "anotherpassword";
           receive_password = "password"
   };

Finally, you must configure all servers in your network to recognize the
services server::

   service {
           name = "services.example.org";
   };
