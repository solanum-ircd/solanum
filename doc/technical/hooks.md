# Hook documentation

Hooks allow modules to execute logic during predefined spots of other module
code or core code. Each function attached to a hook is passed a data pointer
containing contextual information to assist with any logic that function may
need to perform. For examples on how to register a hook function with a
module or define a new hook in a module, see `extensions/example_module.c`.

## Hook list

| Module     | Hook name                      | Description                                               |
|------------|--------------------------------|-----------------------------------------------------------|
| Core       | after_client_exit              | A user has left the server                                |
| Core       | burst_client                   | A user was sent to a remote server during a netjoin       |
| Core       | burst_channel                  | A channel was sent to a remote server during a netjoin    |
| Core       | burst_finished                 | We finished bursting users/channels during a netjoin      |
| Core       | can_join                       | A local user is about to join a channel                   |
| Core       | can_kick                       | A user is about to be kicked from a channel               |
| Core       | can_send                       | Called when checking if a user can talk on a channel      |
| Core       | cap_change                     | A user's client capabilities changed                      |
| Core       | client_exit                    | A user is leaving the server                              |
| Core       | conf_read_end                  | Called after reading conf files on start or rehash        |
| Core       | conf_read_start                | Called before reading conf files on start or rehash       |
| Core       | get_channel_access             | Called when checking a user's privileges on a channel     |
| Core       | introduce_client               | A user has been introduced to the rest of the network     |
| Core       | message_handler                | Determine the handler used to process an incoming message |
| Core       | message_tag                    | Called for each tag on incoming messages                  |
| Core       | new_local_user                 | A user connected locally, before introducing to network   |
| Core       | new_remote_user                | A remote user was introduced, before propagating onward   |
| Core       | outbound_msgbuf                | A message is about to be sent to one or more targets      |
| Core       | priv_change                    | An oper's privset changed                                 |
| Core       | privmsg_channel                | A message or PART is about to be sent to a channel        |
| Core       | privmsg_user                   | A message is about to be sent to a user                   |
| Core       | rehash                         | The server finished rehashing its conf files              |
| Core       | server_introduced              | A server has been introduced to the network (pre-burst)   |
| Core       | server_eob                     | A server has finished processing a netjoin burst from us  |
| Core       | umode_changed                  | The modes for a user have changed                         |
| m_info     | doing_info_conf                | Conf entries have been sent to an oper using INFO         |
| m_invite   | can_invite                     | A local user is about to invite another user to a channel |
| m_invite   | invite                         | A local user is about to be invited to a channel          |
| m_join     | can_create_channel             | A user is about to create a channel                       |
| m_join     | channel_join                   | A user has finished joining a channel                     |
| m_join     | channel_lowerts                | A remote server gave us a lower TS for a channel          |
| m_kill     | can_kill                       | A local oper is about to kill a user                      |
| m_nick     | local_nick_change              | A local user has changed nicknames                        |
| m_nick     | remote_nick_change             | A remote user has changed nicknames                       |
| m_quit     | client_quit                    | A user has quit from the network                          |
| m_services | account_change                 | A user's services account has changed                     |
| m_stats    | doing_stats                    | A user has run STATS                                      |
| m_stats    | doing_stats_show_idle          | Called to determine if send/recv data is shown in STATS l |
| m_trace    | doing_trace_show_idle          | Called to determine if timestamp data is shown in TRACE   |
| m_version  | doing_version_confopts         | Called before sending confopts in VERSION                 |
| m_who      | doing_who_show_idle            | Called to determine if idle time is shown in WHOX         |
| m_whois    | doing_whois                    | A local user has run WHOIS                                |
| m_whois    | doing_whois_channel_visibility | Called to determine which channels to display in WHOIS    |
| m_whois    | doing_whois_global             | A user has run a remote WHOIS targeting this server       |
| m_whois    | doing_whois_show_idle          | Called to determine if idle time is shown in WHOIS        |

## Registering hook functions

To register a function to be called when a hook is executed, you will need to
create an `mapi_hfn_list_av1[]` in your module. Each element of the array
contains the following, in order:

1. The name of the hook as a string. This hook name does not need to exist at
   the time of registration. If the hook doesn't exist and is later defined,
   the hook function will still be called.
2. The function to execute. The function takes a single `void *` parameter and
   has a `void` return value.
3. Optionally, a hook priority. There are six priority levels defined:
   `HOOK_LOWEST`, `HOOK_LOW`, `HOOK_NORMAL`, `HOOK_HIGH`, `HOOK_HIGHEST`, and
   `HOOK_MONITOR`. Hooks are executed in order of *lowest to highest priority*
   with `HOOK_MONITOR` being executed last. If unspecified, normal priority is
   used.

The final array element must contain a `NULL` hook name, to indicate the end
of the array. This array is then passed into the `DECLARE_MODULE_AV2` macro.

## Registering new hooks

A module can state that it is registering additional hooks that it will call
internally. To do so, you will need to create an `mapi_hlist_av1[]` in your
module. Each element of the array contains the following, in order:

1. The name of the hook as a string. It is possible to register the same hook
   name multiple times across different modules without error. Should this
   happen, all modules registering that hook will share the same registration
   and hook function list.
2. An `int *` to store the hook registration number. This pointer should be
   declared as a `static int` at the file scope of the module.

The final array element must contain a `NULL` hook name, to indicate the end
of the array. This array is then passed into the `DECLARE_MODULE_AV2` macro.

## Calling hooks

If a module wishes to call a hook, it uses the `call_hook()` function declared
in `include/hook.h`. It passes the integer registration number of the hook to
this function along with the data pointer to be passed to hook functions.

When calling an existing hook, check the documentation below to ensure that
the data pointer you are passing is compatible with the documented usage
elsewhere, to avoid crashing other modules with unexpected data.

Example:

```c
/* File scope (DECLARE_MODULE_AV2 omitted for brevity): */
static int h_doing_whois;
mapi_hlist_av1[] my_module_hlist = {
    { "doing_whois", &h_doing_whois },
    { NULL, NULL }
};

/* Inside of some relevant function: */
hook_data_client hdata;
hdata.client = source_p;
hdata.target = target_p;
call_hook(h_doing_whois, &hdata);
```

## Hook data structures

Most of the time, the data pointer passed to hook functions is one of the
various structures defined in `include/hook.h`. This is not always the case,
so consult the documentation for the specific hook to find out what it passes
for its data pointer, how it populates its fields, and whether data can flow
back to the calling module.

When defining a new hook, pick one of the predefined structures to use for
your data whenever possible. Think about the interaction model you want
between hooked functions and your module, such as if you want other modules to
be able to pass data back to you by modifying any of the field values.

## Hook details
### account_change

This hook is called after a user's services user (account name) has changed.
The user may be either local or remote. This hook is not called when a user's
services account changes during registration.

Hook data: `hook_cdata *`

Fields:

- client (`struct Client *`): The user whose services account changed
- arg1 (`const char *`): The user's previous services account
- arg2 (`const void *`): Unused (always `NULL`)

The user's updated account is available via `client->user->suser`. If the
user was previously logged out, `arg1` will be an empty string. If the user
just logged out, then `client->user->suser` will be an empty string.

Although unlikely, it is possible for the old and updated account names to be
identical or vary only by casing; no checks are made for this before invoking
the hook. Hook functions that need to operate only if the services account
meaningfully changed should compare the two strings via `irccmp()`.

### after_client_exit

This hook is called after any client (user or server) exits the network. All
cleanup has happened for this client already. If the client is local, it has
been placed on the `dead_list`. Remote clients are not added to `dead_list`.

Hook data: always `NULL`

Given the lack of hook data, if you need to perform cleanup on the specific
client that is exiting, use the `client_exit` hook instead. The
`after_client_exit` hook is suitable if you need to investigate global state
after cleanup has been performed in order to carry out some logic.

### burst_client

This hook is called once for each client we send to another server during a
netjoin burst. The client has already been introduced to the remote server
before the hook is called.

Hook data: `hook_data_client *`

Fields:

- client (`struct Client *`): The server we are sending the burst to
- target (`struct Client *`): The client (user or server) we are bursting

### burst_channel

This hook is called once for each channel we send to another server during a
netjoin burst (n.b. local-only '&' channels are not sent during bursts). The
channel and its member list have already been sent to the remote server before
the hook is called.

Hook data: `hook_data_channel *`

Fields:

- client (`struct Client *`): The server we are sending the burst to
- chptr (`struct Channel *`): The channel we are bursting
- approved (`int`): Unused (always 0)

### burst_finished

This hook is called after we have finished bursting all clients and channels.
We have not yet sent the end of burst marker to the remote server at the time
this hook is called.

Hook data: `hook_data_client *`

Fields:

- client (`struct Client *`): The server we have sent the burst to
- target (`struct Client *`): Unused (always `NULL`)

This hook is ideal for other data that needs to be sent to a remote server
during a netjoin burst, e.g. new global channel types added by modules since
core code only bursts '#' channels.

### can_create_channel

This hook is called when a local user attempts to `JOIN` a nonexistent
channel. Hook functions can reject the channel creation attempt by modifying
the passed-in data.

Hook data: `hook_data_can_create_channel *`

Fields:

- client (`struct Client *`): The user attempting to create the channel
- name (`const char *`): The channel name the user is attempting to create
- approved (`int`): Output field indicating whether this attempt succeeds

The hook function should modify the `approved` field according to whether the
channel creation attempt should succeed or the error that should be sent to
the user should it not succeed. The following values are recognized:

- 0 (default): Allow the channel creation
- `ERR_BADCHANNELKEY`: Prevent the channel creation and send the user a
  message that they have an incorrect channel key (+k)
- `ERR_BANNEDFROMCHAN`: Prevent the channel creation and send the user a
  message that they are banned from the channel (+b)
- `ERR_CHANNELISFULL`: Prevent the channel creation and send the user a
  message that the channel has reached its user limit (+l)
- `ERR_CUSTOM`: Prevent the channel creation and do not send any message to
  the user
- `ERR_INVITEONLYCHAN`: Prevent the channel creation and send the user a
  message that the channel is invite only (+i)
- `ERR_NEEDREGGEDNICK`: Prevent the channel creation and send the user a
  message that a registered nickname is required to join (+r)
- `ERR_THROTTLE`: Prevent the channel creation and send the user a message
  that the join throttle was exceeded and that they should try again (+j)
- Any other value: Prevent the channel creation and send the user a generic
  message that they cannot join the channel with the specified value as the
  numeric sent to the client. The numeric will additionally have the channel
  name as a parameter.

Sending a custom numeric as the value is not recommended as there is no
control over the number of parameters sent nor the error message. Care must be
taken to ensure any numeric sent is compatible with three parameters (client,
channel, and the generic error message "Cannot join channel"). It is better to
set it to `ERR_CUSTOM` and send an appropriate error message to the client
from within your hook function.

### can_invite

This hook is called when a local user invites another user (whether local or
remote) into a channel. It is called even when the user does not have
permissions to issue invites to the channel. However, if the source user is
not on the channel, the invite attempt is rejected before this hook is called.
Hook functions can allow or reject the invite attempt by modifying the
passed-in data.

Hook data: `hook_data_channel_approval *`

Fields:

- client (`struct Client *`): The user sending the INVITE
- chptr (`struct Channel *`): The channel that client is inviting target to
- msptr (`struct membership *`): The client's membership on chptr
- target (`struct Client *`): The user being invited
- approved (`int`): Output field indicating whether this attempt succeeds
- dir (`int`): Unused (always 0)
- modestr (`const char *`): Unused (always `NULL`)
- error (`const char *`): Output field containing an optional error message to
  send to the user

The default value for `approved` is 0 if the client is a channel operator or
if the channel has the +g (free invite) channel mode set. Otherwise, the
default value is 1.

To approve the invite attempt, `approved` must be set to 0. If it is set to
any other value, how the caller behaves depends on whether `error` is also
set. If `approved` is nonzero and `error` is `NULL`, the invite attempt will
be rejected and the client will be sent the numeric `ERR_CHANOPPRIVSNEEDED`.
If `approved` is nonzero and `error` is not `NULL`, the invite attempt will be
rejected and the client will be sent the `approved` value as a numeric with
parameters contained in the `error` string (the client's nick is automatically
inserted as the first parameter and should not be present in the string).

Example:

```c
/* reject all invite attempts with a custom message */
void can_invite_example(void *data_)
{
    hook_data_channel_approval *data = data_;
    char error[BUFSIZE];

    snprintf(error, sizeof(error), "%s %s :%s",
        data->target->name,
        chptr->name,
        "The INVITE command is disabled");

    data->approved = ERR_USERONCHANNEL;
    data->error = &error;
}
```

### can_join

This hook is called when checking whether a local user is able to join a
channel. The target channel will always exist, although it may have just been
created. Newly-created channels passed to this hook will have no members. Hook
functions can allow or reject the join attempt by modifying the passed-in
data.

Hook data: `hook_data_channel *`

Fields:

- client (`struct Client *`): The local user attempting to join the channel
- chptr (`struct Channel *`): The channel being joined
- approved (`int`): Output field indicating whether this attempt succeeds

The hook function should modify the `approved` field according to whether the
join attempt should succeed or the error that should be sent to the user
should it not succeed. The following values are recognized:

- 0: Allow the join
- `ERR_BADCHANNELKEY`: Prevent the join and send the user a message that they
  have an incorrect channel key (+k)
- `ERR_BANNEDFROMCHAN`: Prevent the join and send the user a message that they
  are banned from the channel (+b)
- `ERR_CHANNELISFULL`: Prevent the join and send the user a message that the
  channel has reached its user limit (+l)
- `ERR_CUSTOM`: Prevent the join and do not send any message to the user
- `ERR_INVITEONLYCHAN`: Prevent the join and send the user a message that the
  channel is invite only (+i)
- `ERR_NEEDREGGEDNICK`: Prevent the join and send the user a message that a
  registered nickname is required to join (+r)
- `ERR_THROTTLE`: Prevent the join and send the user a message that the join
  throttle was exceeded and that they should try again (+j)
- Any other value: Prevent the join and send the user a generic message that
  they cannot join the channel with the specified value as the numeric sent to
  the client. The numeric will additionally have the channel name as a
  parameter.

The default value for `approved` will be either 0 or one of the error numerics
specified above (except `ERR_CUSTOM`), depending on if any of them apply based
on the default logic which runs before the hook is called.

Sending a custom numeric as the value is not recommended as there is no
control over the number of parameters sent nor the error message. Care must be
taken to ensure any numeric sent is compatible with three parameters (client,
channel, and the generic error message "Cannot join channel"). It is better to
set it to `ERR_CUSTOM` and send an appropriate error message to the client
from within your hook function.

### can_kick

This hook is called when checking if a local user is able to kick a user (who
may be either local or remote) from a channel. It is only called if the user
has channel operator access to the channel (see also the get_channel_access
hook) and the target is on the channel and is not a service (umode +S). Hook
functions can reject the kick attempt by modifying the passed-in data.

Hook data: `hook_data_channel_approval *`

Fields:

- client (`struct Client *`): The local user issuing the KICK
- chptr (`struct Channel *`): The channel the target is being kicked from
- msptr (`struct membership *`): The client's membership on chptr
- target (`struct Client *`): The user being kicked
- approved (`int`): Output field indicating whether this attempt succeeds
- dir (`int`): Always set to `MODE_ADD` (1)
- modestr (`const char *`): Unused (always `NULL`)
- error (`const char *`): Unused (always `NULL`)

The default value of `approved` is 1, which indicates that the kick is
allowed. Setting `approved` to 0 rejects the kick attempt. No error message is
sent to the client on a rejected attempt; it is up to the hook function to
send any appropriate error message. Any nonzero value also indicates that the
kick is allowed.

### can_kill

This hook is called when a local oper issues a kill for a user, who may be
either local or remote. It is only called after verifying that the oper has
the oper:kill privilege. Hook functions can reject the kill attempt by
modifying the passed-in data.

Hook data: `hook_data_client_approval *`

Fields:

- client (`struct Client *`): The oper issuing the kill
- target (`struct Client *`): The user being killed
- approved (`int`): Output field indicating whether this attempt succeeds

The default value of `approved` is 1, which indicates that the kill is
allowed. Setting `approved` to 0 rejects the kill attempt. No error message is
sent to the client on a rejected attempt; it is up to the hook function to
send any appropriate error message. Any nonzero value also indicates that the
kill is allowed.

### can_send

This hook is called whenever the server needs to check if a user can send a
message to a channel, e.g. a PRIVMSG, NOTICE, TAGMSG, TOPIC change, or PART
reason. This hook is only called for local users with respect to TOPIC and
PART, but is called for both local and remote users for PRIVMSG, NOTICE, and
TAGMSG. This hook is not called if the user is not a member of the channel,
even if the channel allows external messages (-n). Hook functions can allow or
reject the ability to send the message by modifying the passed-in data.

Hook data: `hook_data_channel_approval *`

Fields:

- client (`struct Client *`): The user sending the message
- chptr (`struct Channel *`): The channel the message is being sent to
- msptr (`struct membership *`): The client's membership on chptr
- target (`struct Client *`): Unused (always `NULL`)
- approved (`int`): Output field indicating whether this attempt succeeds
- dir (`int`): Set to `MODE_QUERY` (0) if default logic indicates the user is
  allowed to send to the channel and `MODE_ADD` (1) if default logic indicates
  the user cannot send to the channel
- modestr (`const char *`): Unused (always `NULL`)
- error (`const char *`): Unused (always `NULL`)

The hook function should modify `approved` to indicate whether the user is
allowed to send a message to the channel. The following values are allowed:

- `CAN_SEND_NO`: The user is not allowed to send messages to the channel. This
  is the default value if the user is not opped or voiced on the channel and
  at least one of the following criteria is met:
  - The channel is moderated (+m)
  - The user is banned or quieted from the channel
  - The user is joined to a RESVed channel and is not an oper with the
    oper:general privilege and is not RESV exempt from their I-line
- `CAN_SEND_NONOP`: The user is allowed to send messages to the channel. Flood
  and target change controls will apply to the message. This is the default
  value when none of the criteria in the other two values are met.
- `CAN_SEND_OPV`: The user is allowed to send messages to the channel. Flood
  and target change controls do not apply to the message. This is the default
  value when the user is opped or voiced on the channel.

### cap_change

This hook is called after successfully processing a CAP REQ command from a
local user. The CAP ACK reply has already been sent to the user by the time
this hook is called. This hook is not called if the request is unsuccessful
(meaning a CAP NAK was sent to the user).

Hook data: `hook_data_cap_change *`

Fields:

- client (`struct Client *`): The user who requested caps
- oldcaps (`int`): Bitfield of caps the user had before this request
- add (`int`): Bitfield of caps that this request added
- del (`int`): Bitfield of caps that this request removed

### channel_join

This hook is called after a local user joins a channel. It is not called for
remote users. It is called after the TOPIC and NAMES listing have been sent to
the user and the JOIN has been sent to all other members of the channel.

Hook data: `hook_data_channel_activity *`

Fields:

- client (`struct Client *`): The local user who joined the channel
- chptr (`struct Channel *`): The channel being joined
- key (`const char *`): The key the user specified when joining the channel or
  `NULL` if the user did not specify a key

The key does not necessarily match any key specified by the +k channel mode,
since the can_join hook may have permitted a join despite having an invalid or
missing key. Additionally, a key may be present in the hook data even if the
channel is -k.

### channel_lowerts

This hook is called after the server receives a join message for a remote user
with a lower channel timestamp than what the server has for the channel. This
hook is not called during netjoin bursts or when the remote side believes it
has created a new channel; it is only called a join to an existing channel on
the remote side.

Hook data: `hook_data_channel *`

Fields:

- client (`struct Client *`): The remote user joining the channel
- chptr (`struct Channel *`): The channel being joined
- approved (`int`): Unused (always 0)

At the time the hook is called, client has not yet been added to chptr as a
member, but we have cleared all modes from the local channel. Because this
hook is only called when we receive a JOIN from a remote server and not an
SJOIN, it is not a reliable method of knowing that a local channel has had its
timestamp lowered.

### client_exit

This hook is called when a client is exiting, but before the client has been
removed from the network. The client could be either a user or a server, and
is called for both local and remote clients exiting. It is not called when a
client exits due to exceeding their send queue or if the server encountered an
error while writing to the socket for the client.

Hook data: `hook_data_client_exit *`

Fields:

- local_link (`struct Client *`): The local client originating the exit or
  `NULL` if this exit was generated by the server for internal reasons
- target (`struct Client *`): The client that is exiting
- from (`struct Client *`): The client causing target to exit
- comment (`const char *`): Reason for the exit

Because the `local_link` field can be `NULL` and otherwise does not have
consistent semantics, if you need the local connection for the exiting client
or the source of the exit it is better to use `target->from` or `from->from`,
respectively.

The `from` field is usually the same as `target` (for cases where the client
causes itself to exit) or is `&me` (for cases where the server is exiting the
client). However, this is not always the case. For example, in KILL, `from` is
the oper issuing the KILL command.

The `comment` field contains the quit message that will be sent to other
clients once the exit is processed (after this hook is run). The field is not
editable or replaceable; use the client_quit hook if you need to adjust the
quit message.

### client_quit

This hook is called when a local user sends a QUIT to the server. It is called
before the user is exited. Hook functions can modify the quit reason displayed
to others by modifying the passed-in data.

Hook data: `hook_data_client_quit *`

Fields:

- client (`struct Client *`): The user who is quitting
- reason (`const char *`): Output field containing the quit message
- orig_reason (`const char *`): The quit reason specified by the user, or the
  user's nickname if they did not specify a reason

The `reason` field by default is the same as `orig_reason`. Hook functions can
replace the field with a different pointer value to modify the quit reason. If
a hook function changes the reason, the new reason is not prepended with the
"Quit: " prefix. Hook functions can also set `reason` to `NULL` to default to
a quit reason of "Client Quit".

### conf_read_end

This hook is called after reading ircd.conf on start or during a rehash. All
configuration options have been read and validated and all consequences of
re-reading the configuration (such as updating connection classes) have been
applied.

Hook data: always `NULL`

Modules which define new configuration can make use of this hook to perform
their own validation routines to ensure any module-specific values are set
correctly, or perform other initialization that depends on specific config
values.

### conf_read_start

This hook is called after clearing out any existing configuration (e.g.
deallocating string buffers) but before reading the ircd.conf file on start
or dring a rehash.

Hook data: always `NULL`

Modules which define their own configuration can make use of this hook to
perform cleanup on their own values as needed. String values read from conf
are allocated via `rb_malloc()` so if a module defines new string values it
should call `rb_free()` on those buffers and set them to `NULL` here.

### doing_info_conf

This hook is called when an oper executes the INFO command. The server config
has been sent to that oper, but subsequent information provided by the command
has not yet been sent.

Hook data: `hook_data *`

Fields:

- client (`struct Client *`): The oper executing INFO
- arg1 (`void *`): Unused (always `NULL`)
- arg2 (`void *`): Unused (always `NULL`)

Modules which define their own configuration should make use of this hook to
send information about that configuration to opers executing INFO. See the
example below for the format string that should be used in order to ensure all
module-created options visually align with options printed by default.

Example:

```c
/* This example assumes my_conf_option is a boolean option */
static int my_conf_option;

void send_module_conf(void *data_)
{
    hook_data *data = data_;
    sendto_one(data->client, ":%s %d %s :%-30s %-16s [%s]",
        get_id(&me, data->client), RPL_INFO,
        get_id(data->client, data->client),
        "my_conf_option",
        my_conf_option ? "YES" : "NO",
        "Description of my_conf_option");
}
```

### doing_stats

This hook is called when a user executes the STATS command, before any handler
for the stats letter is executed. If STATS was targeted at a remote server,
the hook is only executed on the server where STATS is ultimately running.
Hook functions can prevent the default handler for the stats letter from being
executed by modifying the passed-in data.

Hook data: `hook_data_int *`

Fields:

- client (`struct Client *`): The user executing STATS
- arg1 (`void *`): Unused (always `NULL`)
- arg2 (`int`): The stat character (upcast from `char` to `int`)
- result (`int`): Output field indicating whether to execute default handler

The `result` field may be set to any nonzero value to stop processing and not
execute any default handlers for the given stats character. Setting `result`
to 0 (which is also the default value) will result in the default handler
being run (or an error given to the client if no default handler for that
character exists). Modules which add additional stats characters should
perform their logic in this hook function, including any output to the client,
and then set `result` to 1. Note that `RPL_ENDOFSTATS` is given to the client
even if default handlers do not execute, so hook functions should not emit
that numeric to the client themselves.

### doing_stats_show_idle

This hook is called when a user executes `STATS l` against a user. Hook
functions can prevent details that would expose that user's idle time on the
network by modifying the passed-in data.

Hook data: `hook_data_client_approval *`

Fields:

- client (`struct Client *`): The user executing STATS l
- target (`struct Client *`): The target of the stats command
- approved (`int`): Output field indicating whether idle information is shown

The default value of `approved` is `WHOIS_IDLE_SHOW`, which displays
information that can be used to determine the target's idle time on the
network. Hook functions can set this to `WHOIS_IDLE_HIDE` instead to make the
output return 0's for that data instead. Setting `approved` to
`WHOIS_IDLE_AUSPEX` or any other nonzero value behaves exactly the same as
`WHOIS_IDLE_SHOW`.

The information reported as 0 when `approved` is set to `WHOIS_IDLE_HIDE` is
the following:

- The number of lines in the target's send queue
- The number of protocol messages sent to the target
- The size in KiB of all data sent to the target, rounded down
- The number of protocol messages received from the target
- The size in KiB of all data received from the target, rounded down
- The number of seconds since we last received a protocol message from the
  target

### doing_trace_show_idle

This hook is called when a user executes TRACE against a user. Hook functions
can prevent details that would expose that user's idle time on the network by
modifying the passed-in data.

Hook data: `hook_data_client_approval *`

Fields:

- client (`struct Client *`): The user executing TRACE
- target (`struct Client *`): The target of the trace command
- approved (`int`): Output field indicating whether idle information is shown

The default value of `approved` is `WHOIS_IDLE_SHOW`, which displays
information that can be used to determine the target's idle time on the
network. Hook functions can set this to `WHOIS_IDLE_HIDE` instead to make the
output return 0's for that data instead. Setting `approved` to
`WHOIS_IDLE_AUSPEX` or any other nonzero value behaves exactly the same as
`WHOIS_IDLE_SHOW`.

The information reported as 0 when `approved` is set to `WHOIS_IDLE_HIDE` is
the following:

- The number of seconds since we last received a protocol message from the
  target
- The number of seconds since the target last sent a PRIVMSG or TAGMSG

### doing_version_confopts

This hook is called when a user executes VERSION. Hook functions can modify
which characters are sent in the RPL_VERSION response comment that indicate
ircd configuration options.

Hook data: `char[256]`

See doc/server-version-info.txt for more information on what each character
in this response represents. The hook data is pre-filled with the default
options supported by solanum according to the referenced document. Modules can
set members of the array to 1 (or any other nonzero value) to have the
associated character show up in the VERSION response. Setting the value to 0
will cause the associated character to be omitted from the VERSION response.

Only the characters A-Z, a-z, and 0-9 will be displayed in VERSION responses;
setting other characters to 1 will have no effect on the output. Characters
will be output in sorted order, with letters first followed by numbers. If a
letter has both uppercase and lowercase variants set, the uppercase letter
will be output before the lowercase letter.

Example:

```c
void doing_version_confopts_example(void *data)
{
    char *opts = data;
    /* Causes Q to additionally appear in the VERSION response */
    opts['Q'] = 1;
}
```

### doing_who_show_idle

This hook is called when a user executes an extended WHO (WHOX) query while
specifying the `l` flag to retrieve idle time. Hook functions can prevent the
details that would expose a user's idle time on the network by modifying the
passed-in data. The hook is called once for each user returned by the WHO.

Hook data: `hook_data_client_approval *`

Fields:

- client (`struct Client *`): The user executing WHO
- target (`struct Client *`): The target matching the WHO query
- approved (`int`): Output field indicating whether idle information is shown

The default value of `approved` is `WHOIS_IDLE_SHOW`, which displays the
target's idle time in seconds if the target is a local user. If the target is
a remote user, a value of 0 will always be returned regardless of the value of
`approved`, but the hook functions will nonetheless be called. Hook functions
can set this to `WHOIS_IDLE_HIDE` instead to make the output always return 0.
Setting `approved` to `WHOIS_IDLE_AUSPEX` or any other nonzero value behaves
exactly the same as `WHOIS_IDLE_SHOW`.

### doing_whois

This hook is called when a local user performs a WHOIS on someone else. The
target of the WHOIS will always be a local client. The hook is called after
all regular WHOIS lines have been sent to the user, but before the
`RPL_ENDOFWHOIS` numeric is sent to the user.

Hook data: `hook_data_client *`

Fields:

- client (`struct Client *`): The user executing WHOIS
- target (`struct Client *`): The target of the WHOIS command

Hook functions can use this hook to send additional lines to the client as a
part of the WHOIS response.

### doing_whois_channel_visibility

This hook is called for each channel the target of a WHOIS command belongs to.
The target will always be a local client. Hook functions can prevent the
channel from being displayed in the WHOIS output by modifying the passed-in
data.

Hook data: `hook_data_channel_visibility *`

Fields:

- client (`struct Client *`): The user executing WHOIS
- target (`struct Client *`): The target of the WHOIS command
- chptr (`struct Channel *`): The channel being displayed
- clientms (`struct membership *`): The client's membership in the channel,
  or `NULL` if they are not in the channel.
- targetms (`struct membership *`): The target's membership in the channel
- approved (`int`): Output field indicating whether the channel is shown

The default value of `approved` is 1 if client is a member of the channel or
if the channel is public (not channel mode `+s` or `+p`) and 0 otherwise. If
`approved` is set to 1, the channel will be displayed in the WHOIS output.
Otherwise, the channel is either hidden from the output for regular WHOIS or
prefixed with the `!` character for operspy WHOIS.

### doing_whois_global

This hook is called when a remote user performs a WHOIS on someone else. The
target of the WHOIS in such a case will always be a local client. The hook is
called after all regular WHOIS lines have been sent to the user, but before
the `RPL_ENDOFWHOIS` numeric is sent to the user.

Hook data: `hook_data_client *`

Fields:

- client (`struct Client *`): The user executing WHOIS
- target (`struct Client *`): The target of the WHOIS command

Hook functions can use this hook to send additional lines to the client as a
part of the WHOIS response.

### doing_whois_show_idle

This hook is called when a user runs WHOIS against a local target. The user
may be either local or remote. Hook functions can prevent the details that
would expose the target's idle time on the network by modifying the passed-in
data.

Hook data: `hook_data_client_approval *`

Fields:

- client (`struct Client *`): The user executing WHOIS
- target (`struct Client *`): The target of the WHOIS command
- approved (`int`): Output field indicating whether idle information is shown

The default value of `approved` is `WHOIS_IDLE_SHOW`. The output given to the
user varies based on the value of `approved` and whether the target has the
`+I` user mode (ostensibly from extensions/umode_hide_idle_time):

| `approved`          | target `+I` | result                                                                                                                                       |
|---------------------|-------------|----------------------------------------------------------------------------------------------------------------------------------------------|
| `WHOIS_IDLE_SHOW`   | -           | idle time shown                                                                                                                              |
| `WHOIS_IDLE_HIDE`   | yes         | idle time hidden, client told the target is hiding their idle time                                                                           |
| `WHOIS_IDLE_HIDE`   | no          | idle time hidden, client told the target's idle time is hidden because the client is +I                                                      |
| `WHOIS_IDLE_AUSPEX` | yes         | idle time shown, client told the target is hiding their idle time but the client has auspex and can see it anyway                            |
| `WHOIS_IDLE_AUSPEX` | no          | idle time shown, client told the target's idle time would be hidden because the client is +I but the client has auspex and can see it anyway |

Any values for `approved` other than the above are treated equivalently to
`WHOIS_IDLE_AUSPEX`. The user mode `+I` is checked irrespective of which
module is providing it, if any.

### get_channel_access

This hook is called whenever a client's access to a channel needs to be
verified before the action is allowed to proceed. Hook functions can alter
the user's ability to perform the action by modifying the passed-in data.

Hook data: `hook_data_channel_approval *`

Fields:

- client (`struct Client *`): The client performing a channel action
- chptr (`struct Channel *`): The channel being checked for client's access
- msptr (`struct membership *`): The client's membership on chptr
- approved (`int`): Output field indicating client's access to the channel
- dir (`int`): One of `MODE_ADD`, `MODE_QUERY`, or `MODE_OP_QUERY`
- modestr (`const char *`): The canonicalized set of channel mode changes as
  would be passed to a MODE command if this hook is called as a result of
  channel mode changes, otherwise `NULL`
- error (`const char *`): Unused (always `NULL`)

This hook is called under the following circumstances:

- Retrieving the values of a queryable mode for a channel
  - The following modes are considered queryable: `beIfkq`
  - The following modes are considered privileged while querying: `eI`
  - Module-defined channel modes can be marked queryable by setting either
    `CHM_QUERYABLE` or `CHM_CAN_QUERY` when defining the mode and
    privileged by setting `CHM_OPS_QUERY` when defining the mode
  - `dir` will be `MODE_OP_QUERY` if at least one of the queried modes in
    `modestr` is a privileged mode and `MODE_QUERY` otherwise
  - `modestr` will be the canonicalized set of channel modes being queried
- Updating (adding or removing) modes for a channel
  - If the mode update is combined with a privileged mode query, e.g.
    `MODE #channel +m=I`, `dir` will be `MODE_OP_QUERY`; otherwise, `dir` will
    always be `MODE_ADD`, regardless of whether modes are being added,
    removed, or both
  - `modestr` will be the canonicalized set of channel modes being updated
- Forwarding (channel mode `+f` or ban forwards) to another channel
  - The hook is called for the target channel to verify the client has
    operator status on it and is in addition to the hook invocation for
    updating modes on the source channel
  - `dir` will always be `MODE_QUERY` and `modestr` will always be `NULL`
- Setting the topic for a channel
  - `dir` will always be `MODE_ADD` and `modestr` will always be `NULL`
- Kicking a user from a channel
  - `dir` will always be `MODE_ADD` and `modestr` will always be `NULL`
- Removing a user from a channel (via extensions/m_remove)
  - `dir` will always be `MODE_ADD` and `modestr` will always be `NULL`

For mode queries and updates, the direction in `modestr` is always explicitly
specified by prefixing with `=` for queries, `+` for additions, and `-` for
removals. For example, a user sending `MODE #channel b` to query the ban list
would cause `modestr` to be `=b`. Unrecognized or invalid mode letters are
removed from `modestr`, e.g. attempts to query non-queryable modes. Multiple
adjacent modes with the same direction will not duplicate the prefix. For
example, a user sending `MODE #channel +m+z` would cause `modestr` to be `+mz`
but a user sending `MODE #channel +m-R+z` would cause `modestr` to remain as
`+m-R+z` (i.e. modes are not rearranged from what the user specified).
`modestr` will additionally contain all relevant arguments to modes, separated
by spaces. These transformations are what the documentation above refers to
when it talks about the canonicalized set of channel modes.

The default value of `approved` is `CHFL_CHANOP` if client is a member of
the channel and has channel operator status on the channel and `CHFL_PEON`
otherwise. In general, for everything except querying non-privileged modes,
the action will be denied if `approved` is set to anything lower than
`CHFL_CHANOP` (e.g. `CHFL_PEON` or `CHFL_VOICE`).

### introduce_client

This hook is called whenever a client is introduced to the network. The client
has already been fully registered before this hook is called, and the client
may be either local or remote.

Hook data: `hook_data_client *`

Fields:

- client (`struct Client *`): The local connection for the introduced client
- target (`struct Client *`): The client being introduced

For local clients, client and target will point to the same place. For remote
clients, client will be the locally-connected server that is introducing the
client to us.

### invite

This hook is called whenever a local user is invited to a channel, after the
can_invite hook has already been executed on the sender's server. The user
sending the invite may be either local or remote. Hook functions can allow or
reject the invite attempt by modifying the passed-in data.

Hook data: `hook_data_channel_approval *`

Fields:

- client (`struct Client *`): The user sending the INVITE
- chptr (`struct Channel *`): The channel that client is inviting target to
- msptr (`struct membership *`): The client's membership on chptr
- target (`struct Client *`): The user being invited
- approved (`int`): Output field indicating whether this attempt succeeds
- dir (`int`): Unused (always 0)
- modestr (`const char *`): Unused (always `NULL`)
- error (`const char *`): Output field containing an optional error message to
  send to the user

The default value for `approved` is 0. To approve the invite attempt,
`approved` must be set to 0. If it is set to any other value, how the caller
behaves depends on whether `error` is also set. If `approved` is nonzero and
`error` is `NULL`, the invite attempt will silently fail. If `approved` is
nonzero and `error` is not `NULL`, the invite attempt will be rejected and the
client will be sent the `approved` value as a numeric with parameters
contained in the `error` string (the client's nick is automatically inserted
as the first parameter and should not be present in the string).

### local_nick_change

This hook is called whenever a local user changes their nickname. This hook is
only called when the nickname change would succeed, and timing-wise occurs
after `RPL_MONOFFLINE` is sent for the old nickname but before `RPL_MONONLINE`
is sent for the new nickname and before the nick change is propagated to local
users and the rest of the network.

Hook data: `hook_cdata *`

Fields:

- client (`struct Client *`): The user whose nick is changing
- arg1 (`const char *`): The user's previous nickname
- arg2 (`const char *`): The user's new nickname

### message_handler

This hook is called for every incoming message processed by the server. Hook
functions can alter the message handler ordinarily called by the ircd for the
client by modifying the passed-in data.

Hook data: `hook_data *`

Fields:

- client (`struct Client *`): The client sending the message
- arg1 (`struct MsgBuf *`): The message
- arg2 (`struct MessageEntry *`): Output field for the function used to handle
  the message

The default value for `arg2` is the defined message handler for the command
based on the client's status (unregistered, local user, remote user, server,
encap, oper). Hook functions can replace this with a different handler by
setting `arg2->handler` to a different pointer. `arg2->min_args` may be
modified as well if the new handler supports a different minimum number of
arguments compared to the default handler.

### message_tag

This hook is called for every message tag present in an incoming message. Hook
functions can reject the tag or message or update the tag by modifying the
passed-in data.

Hook data: `hook_data_message_tag *`

Fields:

- client (`struct Client *`): The local connection the message came from
- source (`struct Client *`): The client originating the message
- key (`const char *`): The name of the message tag
- value (`const char *`): The value of the message tag, or `NULL` if a value
  was not specified with this tag
- capmask (`unsigned int`): Output field indicating the client capabilities a
  user must possess in order to receive this tag in server-sent messages
- message (`const struct MsgBuf *`): The full command received, including all
  tags and parameters
- approved (`int`): Output field indicating whether this message tag should be
  retained before passing the message off to the appropriate handler

The default value of `approved` is `MESSAGE_TAG_REMOVE`, which causes the tag
to be stripped from the message before being passed to the handler. Hook
functions can instead set this to `MESSAGE_TAG_ALLOW` to include the tag in
the message or `MESSAGE_TAG_DROP` to prevent the entire message from being
processed. When using `MESSAGE_TAG_DROP` it is the hook function's
responsibility to provide any relevant error messages to the source. Using any
value outside of these will be treated equivalently to `MESSAGE_TAG_REMOVE`.

The default value of `capmask` is `CLICAP_MESSAGE_TAGS`. Hook functions can
modify this to be a different client capability or bitwise combination of
client capabilities. While there is no guarantee that modifying `capmask`
will accomplish anything, some message handlers pass some or all incoming tags
to outgoing messages, and setting this appropriately allows such tags to be
propagated appropriately.

Finally, `key` and `value` may both be set to different pointer values to
adjust the tag's key and value passed to the message handler. The pointed-to
buffers are not copied and must persist for the lifetime of the message
handler. Setting `value` to `NULL` will strip the value from the tag.

### new_local_user

This hook is called after a local user has completed connection registration,
but before it is formally introduced to the server or network. This hook is
only called for users; see server_introduced for the introduction of new
server connections.

Hook data: `struct Client *`

The hook data is the user being introduced. Hook functions can safely kill
this client should the connection attempt not be allowed to proceed.

### new_remote_user

This hook is called after a remote user has been introduced to this server.
This hook is only called for users; see server_introduced for the introduction
of new server connections.

Hook data: `struct Client *`

The hook data is the user being introduced. Unlike new_local_user, hook
functions must *not* attempt to disconnect clients during this hook. The user
will still be introduced to downstream servers after this hook completes
running and disconnection attempts will therefore introduce state
desynchronization between servers. If a remote user needs to be disconnected
as early as possible after having been introduced, the introduce_client hook
is the best spot to do that.

### outbound_msgbuf

This hook is called when constructing a message to be sent from the server.
Hook functions can manipulate the message tags that would be sent by modifying
the passed-in data. This hook is only called once per outbound message,
regardless of how many clients the message will be sent to.

Hook data: `hook_data *`

Fields:

- client (`struct Client *`): The client sending the message
- arg1 (`struct MsgBuf *`): The message buffer being sent
- arg2 (`void *`): Unused (always `NULL`)

Hook functions can inspect `arg1` and modify it. Any modifications such as the
addition or removal of message tags will be sent to the targets of the
message. To add message tags, call msgbuf_append_tag. To remove tags, set the
tag's `capmask` to 0. Changing other aspects of the MsgBuf such as the origin
or parameter array is not recommended and may not function as expected.

The following send functions currently do not call this hook:

- kill_client
- kill_client_serv_butone

### priv_change

This hook is called whenever an oper's privilege set changes. This could be
due to a user successfully becoming an oper, deopering, or an ircd rehash.

Hook data: `hook_data_priv_change *`

Fields:

- client (`struct Client *`): The oper whose privileges have changed
- old (`struct PrivilegeSet *`): The oper's previous privset, or `NULL` if
  they are opering up
- new (`struct PrivilegeSet *`): The oper's new privset, or `NULL` if they
  have deopered
- added (`const struct PrivilegeSet *`): The set of privileges added
- removed (`const struct PrivilegeSet *`): The set of privileges removed
- unchanged (`const struct PrivilegeSet *`): Privileges which remained intact

When ircd.conf is rehashed, this hook will be invoked for every local oper,
even if there was no change to the privileges in their privset. Additionally,
this hook will not be invoked upon rehash for remote opers even if there were
changes to the privileges in their privset.

### privmsg_channel

This hook is called whenever a user sends a message to a channel or is parting
a channel. Hook functions can reject the message attempt or alter the message
text by modifying the passed-in data. This hook is called for messages sent by
both local and remote users.

Hook data: `hook_data_privmsg_channel *`

Fields:

- msgtype (`enum message_type`): The type of message being sent
- source_p (`struct Client *`): The user sending the message
- chptr (`struct Channel *`): The channel the message is being sent to
- text (`const char *`): The message text
- approved (`int`): Output field indicating whether the message is allowed to
  be sent
- msgbuf (`struct MsgBuf *`): The full message, including tags, being sent

The default value for `approved` is 0, which allows the message to be sent.
Setting `approved` to any nonzero value silently drops the message for
`MESSAGE_TYPE_PRIVMSG`, `MESSAGE_TYPE_NOTICE`, and `MESSAGE_TYPE_TAGMSG` and
causes the user to part without a reason parameter for `MESSAGE_TYPE_PART`.
It is up to the hook function to provide any relevant error output to the user
when messages are rejected.

Hook functions may set `text` to a different pointer value. If this is done,
the new text will be used for message content instead of the original text.
Hook functions may additionally manipulate the tags present in `msgbuf`; any
modifications to tags will be reflected in the message sent to the channel but
other modifications will not be effective.

### privmsg_user

This hook is called whenever a user sends a message to a channel. Hook
functions can reject the message attempt or alter the message text by
modifying the passed-in data. This hook is called for messages sent by both
local and remote users.

Hook data: `hook_data_privmsg_user *`

Fields:

- msgtype (`enum message_type`): The type of message being sent
- source_p (`struct Client *`): The user sending the message
- target_p (`struct Client *`): The user the message is being sent to
- text (`const char *`): The message text
- approved (`int`): Output field indicating whether the message is allowed to
  be sent
- msgbuf (`struct MsgBuf *`): The full message, including tags, being sent

The default value for `approved` is 0, which allows the message to be sent.
Setting `approved` to any nonzero value silently drops the message. It is up
to the hook function to provide any relevant error output to the user when
messages are rejected.

Hook functions may set `text` to a different pointer value. If this is done,
the new text will be used for message content instead of the original text.
Hook functions may additionally manipulate the tags present in `msgbuf`; any
modifications to tags will be reflected in the message sent to the user but
other modifications will not be effective.

### rehash

This hook is called after an ircd.conf rehash operation has completed.

Hook data: `hook_data_rehash *`

Fields:

- signal (`bool`): Whether the rehash was the result of a signal or an oper
  command

The value of `signal` will be true if the rehash was the result of a SIGHUP
sent to the ircd process and false if the rehash was the result of a REHASH or
MODRESTART command.

### remote_nick_change

This hook is called whenever a remote user changes their nickname. This hook
is only called when the nickname change would succeed, and timing-wise occurs
after `RPL_MONOFFLINE` is sent for the old nickname but before `RPL_MONONLINE`
is sent for the new nickname and before the nick change is propagated to local
users and the rest of the network.

Hook data: `hook_cdata *`

Fields:

- client (`struct Client *`): The user whose nick is changing
- arg1 (`const char *`): The user's previous nickname
- arg2 (`const char *`): The user's new nickname

### server_introduced

This hook is called whenever a server is introduced to the network. The server
may be directly connected or remote.

Hook data: `hook_data_client *`

Fields:

- client (`struct Client *`): The server introducing this server
- target (`struct Client *`): The server being introduced

For local connections, `client` will be `&me`.

### server_eob

This hook is called after a netjoin burst has been processed by a server. The
server may be either directly connected or remote.

Hook data: `struct Client *`

The hook data is the server that has indicated it has finished processing any
netjoin burst data sent by us. The EOB flag has already been set on source_p
by the time this hook is called.

### umode_changed

This hook is called after a user is introduced and after any changes to their
user modes, such as by the MODE, OPER, or DEHELPER commands. Any mode changes
have already been applied at the time this hook is called.

Hook data: `hook_data_umode_changed *`

Fields:

- client (`struct Client *`): The user whose modes have changed
- oldumodes (`unsigned int`): The user's old user modes
- oldsnomask (`unsigned int`): The user's old server notice mask

Because any mode changes have already been applied, hook functions can check
the current user modes or snomask for the user to determine what has changed.
