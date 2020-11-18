/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_info.c: Sends information about the server.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

#include "stdinc.h"
#include "m_info.h"
#include "channel.h"
#include "client.h"
#include "match.h"
#include "ircd.h"
#include "hook.h"
#include "numeric.h"
#include "s_serv.h"
#include "s_user.h"
#include "send.h"
#include "s_conf.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_newconf.h"

static const char info_desc[] =
	"Provides the INFO command for retrieving server copyright, credits, and other info";

static void send_conf_options(struct Client *source_p);
static void send_birthdate_online_time(struct Client *source_p);
static void send_info_text(struct Client *source_p);
static void info_spy(struct Client *);

static void m_info(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static void mo_info(struct MsgBuf *, struct Client *, struct Client *, int, const char **);

struct Message info_msgtab = {
	"INFO", 0, 0, 0, 0,
	{mg_unreg, {m_info, 0}, {mo_info, 0}, mg_ignore, mg_ignore, {mo_info, 0}}
};

int doing_info_hook;

mapi_clist_av1 info_clist[] = { &info_msgtab, NULL };
mapi_hlist_av1 info_hlist[] = {
	{ "doing_info",		&doing_info_hook },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(info, NULL, NULL, info_clist, info_hlist, NULL, NULL, NULL, info_desc);

enum info_output_type {
	OUTPUT_STRING,     /* Output option as %s w/ dereference */
	OUTPUT_STRING_PTR, /* Output option as %s w/out deference */
	OUTPUT_DECIMAL,    /* Output option as decimal (%d) */
	OUTPUT_BOOLEAN,    /* Output option as "ON" or "OFF" */
	OUTPUT_BOOLEAN_YN, /* Output option as "YES" or "NO" */
	OUTPUT_INTBOOL,    /* BOOLEAN encoded as an int */
	OUTPUT_INTBOOL_YN, /* BOOLEAN_YN encoded as an int */
	OUTPUT_YESNOMASK,  /* Output option as "YES/NO/MASKED" */
	OUTPUT_STATSL,     /* Output as "YES/NO/SELF" */
};

#define INFO_STRING(ptr)      OUTPUT_STRING,     .option.string_p = (ptr)
#define INFO_STRING_PTR(ptr)  OUTPUT_STRING_PTR, .option.string = (ptr)
#define INFO_BOOLEAN(ptr)     OUTPUT_BOOLEAN,    .option.bool_ = (ptr)
#define INFO_BOOLEAN_YN(ptr)  OUTPUT_BOOLEAN_YN, .option.bool_ = (ptr)
#define INFO_INTBOOL(ptr)     OUTPUT_INTBOOL,    .option.int_ = (ptr)
#define INFO_INTBOOL_YN(ptr)  OUTPUT_INTBOOL_YN, .option.int_ = (ptr)
#define INFO_YESNOMASK(ptr)   OUTPUT_YESNOMASK,  .option.int_ = (ptr)
#define INFO_DECIMAL(ptr)     OUTPUT_DECIMAL,    .option.int_ = (ptr)
#define INFO_STATSL(ptr)      OUTPUT_STATSL,     .option.statsl = (ptr)

struct InfoStruct
{
	const char *name;
	const char *desc;
	enum info_output_type output_type;
	union
	{
		const int *int_;
		const bool *bool_;
		char *const *string_p;
		const char *string;
		const enum stats_l_oper_only *statsl;
	} option;
};

/* *INDENT-OFF* */
static struct InfoStruct info_table[] = {

	{
		"opers_see_all_users",
		"Farconnect notices available or operspy accountability limited",
		INFO_BOOLEAN(&opers_see_all_users)
	},
	{
		"max_connections",
		"Max number connections",
		INFO_DECIMAL(&maxconnections),
	},
	{
		"anti_nick_flood",
		"NICK flood protection",
		INFO_INTBOOL(&ConfigFileEntry.anti_nick_flood),
	},
	{
		"anti_spam_exit_message_time",
		"Duration a client must be connected for to have an exit message",
		INFO_DECIMAL(&ConfigFileEntry.anti_spam_exit_message_time),
	},
	{
		"caller_id_wait",
		"Minimum delay between notifying UMODE +g users of messages",
		INFO_DECIMAL(&ConfigFileEntry.caller_id_wait),
	},
	{
		"client_exit",
		"Prepend 'Quit:' to user QUIT messages",
		INFO_INTBOOL(&ConfigFileEntry.client_exit),
	},
	{
		"client_flood_max_lines",
		"Number of lines before a client Excess Flood's",
		INFO_DECIMAL(&ConfigFileEntry.client_flood_max_lines),
	},
	{
		"client_flood_burst_rate",
		"Maximum lines per second during flood grace period",
		INFO_DECIMAL(&ConfigFileEntry.client_flood_burst_rate),
	},
	{
		"client_flood_burst_max",
		"Number of lines to process at once before delaying",
		INFO_DECIMAL(&ConfigFileEntry.client_flood_burst_max),
	},
	{
		"client_flood_message_num",
		"Number of messages to allow per client_flood_message_time outside of burst",
		INFO_DECIMAL(&ConfigFileEntry.client_flood_message_num),
	},
	{
		"client_flood_message_time",
		"Time to allow per client_flood_message_num outside of burst",
		INFO_DECIMAL(&ConfigFileEntry.client_flood_message_time),
	},
	{
		"post_registration_delay",
		"Time to wait before processing commands from a new client",
		INFO_DECIMAL(&ConfigFileEntry.post_registration_delay),
	},
	{
		"connect_timeout",
		"Connect timeout for connections to servers",
		INFO_DECIMAL(&ConfigFileEntry.connect_timeout),
	},
	{
		"default_ident_timeout",
		"Amount of time the server waits for ident responses from clients",
		INFO_DECIMAL(&ConfigFileEntry.default_ident_timeout),
	},
	{
		"default_floodcount",
		"Startup value of FLOODCOUNT",
		INFO_DECIMAL(&ConfigFileEntry.default_floodcount),
	},
	{
		"default_adminstring",
		"Default adminstring at startup.",
		INFO_STRING(&ConfigFileEntry.default_adminstring),
	},
	{
		"default_operstring",
		"Default operstring at startup.",
		INFO_STRING(&ConfigFileEntry.default_operstring),
	},
	{
		"servicestring",
		"String shown in whois for opered services.",
		INFO_STRING(&ConfigFileEntry.servicestring),
	},
	{
		"drain_reason",
		"Message to quit users with if this server is draining.",
		INFO_STRING(&ConfigFileEntry.drain_reason),
	},
	{
		"disable_auth",
		"Controls whether auth checking is disabled or not",
		INFO_INTBOOL_YN(&ConfigFileEntry.disable_auth),
	},
	{
		"disable_fake_channels",
		"Controls whether bold etc are disabled for JOIN",
		INFO_INTBOOL_YN(&ConfigFileEntry.disable_fake_channels),
	},
	{
		"dots_in_ident",
		"Number of permissible dots in an ident",
		INFO_DECIMAL(&ConfigFileEntry.dots_in_ident),
	},
	{
		"failed_oper_notice",
		"Inform opers if someone /oper's with the wrong password",
		INFO_INTBOOL(&ConfigFileEntry.failed_oper_notice),
	},
	{
		"fname_userlog",
		"User log file",
		INFO_STRING(&ConfigFileEntry.fname_userlog),
	},
	{
		"fname_fuserlog",
		"Failed user log file",
		INFO_STRING(&ConfigFileEntry.fname_fuserlog),
	},

	{
		"fname_operlog",
		"Operator log file",
		INFO_STRING(&ConfigFileEntry.fname_operlog),
	},
	{
		"fname_foperlog",
		"Failed operator log file",
		INFO_STRING(&ConfigFileEntry.fname_foperlog),
	},
	{
		"fname_serverlog",
		"Server connect/disconnect log file",
		INFO_STRING(&ConfigFileEntry.fname_serverlog),
	},
	{
		"fname_killlog",
		"KILL log file",
		INFO_STRING(&ConfigFileEntry.fname_killlog),
	},
	{
		"fname_klinelog",
		"KLINE etc log file",
		INFO_STRING(&ConfigFileEntry.fname_klinelog),
	},
	{
		"fname_operspylog",
		"Oper spy log file",
		INFO_STRING(&ConfigFileEntry.fname_operspylog),
	},
	{
		"fname_ioerrorlog",
		"IO error log file",
		INFO_STRING(&ConfigFileEntry.fname_ioerrorlog),
	},
	{
		"global_snotices",
		"Send out certain server notices globally",
		INFO_INTBOOL_YN(&ConfigFileEntry.global_snotices),
	},
	{
		"hide_error_messages",
		"Hide ERROR messages coming from servers",
		INFO_YESNOMASK(&ConfigFileEntry.hide_error_messages),
	},
	{
		"hide_spoof_ips",
		"Hide IPs of spoofed users",
		INFO_INTBOOL_YN(&ConfigFileEntry.hide_spoof_ips),
	},
	{
		"kline_reason",
		"K-lined clients sign off with this reason",
		INFO_STRING(&ConfigFileEntry.kline_reason),
	},
	{
		"dline_with_reason",
		"Display D-line reason to client on disconnect",
		INFO_INTBOOL_YN(&ConfigFileEntry.dline_with_reason),
	},
	{
		"kline_with_reason",
		"Display K-line reason to client on disconnect",
		INFO_INTBOOL_YN(&ConfigFileEntry.kline_with_reason),
	},
	{
		"hide_tkdline_duration",
		"Hide \"Temporary K-line 123 min.\" from user K/D-lline reasons",
		INFO_INTBOOL_YN(&ConfigFileEntry.hide_tkdline_duration),
	},
	{
		"max_accept",
		"Maximum nicknames on accept list",
		INFO_DECIMAL(&ConfigFileEntry.max_accept),
	},
	{
		"max_nick_changes",
		"NICK change threshold setting",
		INFO_DECIMAL(&ConfigFileEntry.max_nick_changes),
	},
	{
		"max_nick_time",
		"NICK flood protection time interval",
		INFO_DECIMAL(&ConfigFileEntry.max_nick_time),
	},
	{
		"max_targets",
		"The maximum number of PRIVMSG/NOTICE targets",
		INFO_DECIMAL(&ConfigFileEntry.max_targets),
	},
	{
		"min_nonwildcard",
		"Minimum non-wildcard chars in K lines",
		INFO_DECIMAL(&ConfigFileEntry.min_nonwildcard),
	},
	{
		"min_nonwildcard_simple",
		"Minimum non-wildcard chars in xlines/resvs",
		INFO_DECIMAL(&ConfigFileEntry.min_nonwildcard_simple),
	},
	{
		"network_name",
		"Network name",
		INFO_STRING(&ServerInfo.network_name),
	},
	{
		"nick_delay",
		"Delay nicks are locked for on split",
		INFO_DECIMAL(&ConfigFileEntry.nick_delay),
	},
	{
		"no_oper_flood",
		"Disable flood control for operators",
		INFO_INTBOOL(&ConfigFileEntry.no_oper_flood),
	},
	{
		"non_redundant_klines",
		"Check for and disallow redundant K-lines",
		INFO_INTBOOL(&ConfigFileEntry.non_redundant_klines),
	},
	{
		"operspy_admin_only",
		"Send +Z operspy notices to admins only",
		INFO_INTBOOL(&ConfigFileEntry.operspy_admin_only),
	},
	{
		"operspy_dont_care_user_info",
		"Remove accountability and some '!' requirement from non-channel operspy",
		INFO_INTBOOL(&ConfigFileEntry.operspy_dont_care_user_info),
	},
	{
		"pace_wait",
		"Minimum delay between uses of certain commands",
		INFO_DECIMAL(&ConfigFileEntry.pace_wait),
	},
	{
		"pace_wait_simple",
		"Minimum delay between less intensive commands",
		INFO_DECIMAL(&ConfigFileEntry.pace_wait_simple),
	},
	{
		"ping_cookie",
		"Require ping cookies to connect",
		INFO_INTBOOL(&ConfigFileEntry.ping_cookie),
	},
	{
		"reject_after_count",
		"Client rejection threshold setting",
		INFO_DECIMAL(&ConfigFileEntry.reject_after_count),
	},
	{
		"reject_ban_time",
		"Client rejection time interval",
		INFO_DECIMAL(&ConfigFileEntry.reject_ban_time),
	},
	{
		"reject_duration",
		"Client rejection cache duration",
		INFO_DECIMAL(&ConfigFileEntry.reject_duration),
	},
	{
		"short_motd",
		"Do not show MOTD; only tell clients they should read it",
		INFO_INTBOOL_YN(&ConfigFileEntry.short_motd),
	},
	{
		"stats_e_disabled",
		"STATS e output is disabled",
		INFO_INTBOOL_YN(&ConfigFileEntry.stats_e_disabled),
	},
	{
		"stats_c_oper_only",
		"STATS C output is only shown to operators",
		INFO_INTBOOL_YN(&ConfigFileEntry.stats_c_oper_only),
	},
	{
		"stats_i_oper_only",
		"STATS I output is only shown to operators",
		INFO_YESNOMASK(&ConfigFileEntry.stats_i_oper_only),
	},
	{
		"stats_k_oper_only",
		"STATS K output is only shown to operators",
		INFO_YESNOMASK(&ConfigFileEntry.stats_k_oper_only),
	},
	{
		"stats_l_oper_only",
		"STATS l/L output is only shown to operators",
		INFO_STATSL(&ConfigFileEntry.stats_l_oper_only),
	},
	{
		"stats_o_oper_only",
		"STATS O output is only shown to operators",
		INFO_INTBOOL_YN(&ConfigFileEntry.stats_o_oper_only),
	},
	{
		"stats_P_oper_only",
		"STATS P is only shown to operators",
		INFO_INTBOOL_YN(&ConfigFileEntry.stats_P_oper_only),
	},
	{
		"stats_y_oper_only",
		"STATS Y is only shown to operators",
		INFO_INTBOOL_YN(&ConfigFileEntry.stats_y_oper_only),
	},
	{
		"throttle_count",
		"Connection throttle threshold",
		INFO_DECIMAL(&ConfigFileEntry.throttle_count),
	},
	{
		"throttle_duration",
		"Connection throttle duration",
		INFO_DECIMAL(&ConfigFileEntry.throttle_duration),
	},
	{
		"tkline_expire_notices",
		"Notices given to opers when tklines expire",
		INFO_INTBOOL(&ConfigFileEntry.tkline_expire_notices),
	},
	{
		"ts_max_delta",
		"Maximum permitted TS delta from another server",
		INFO_DECIMAL(&ConfigFileEntry.ts_max_delta),
	},
	{
		"ts_warn_delta",
		"Maximum permitted TS delta before displaying a warning",
		INFO_DECIMAL(&ConfigFileEntry.ts_warn_delta),
	},
	{
		"warn_no_nline",
		"Display warning if connecting server lacks connect block",
		INFO_INTBOOL(&ConfigFileEntry.warn_no_nline),
	},
	{
		"use_propagated_bans",
		"KLINE sets fully propagated bans",
		INFO_INTBOOL(&ConfigFileEntry.use_propagated_bans),
	},
	{
		"max_ratelimit_tokens",
		"The maximum number of tokens that can be accumulated for executing rate-limited commands",
		INFO_DECIMAL(&ConfigFileEntry.max_ratelimit_tokens),
	},
	{
		"away_interval",
		"The minimum time between aways",
		INFO_DECIMAL(&ConfigFileEntry.away_interval),
	},
	{
		"tls_ciphers_oper_only",
		"TLS cipher strings are hidden in whois for non-opers",
		INFO_INTBOOL_YN(&ConfigFileEntry.tls_ciphers_oper_only),
	},
	{
		"default_split_server_count",
		"Startup value of SPLITNUM",
		INFO_DECIMAL(&ConfigChannel.default_split_server_count),
	},
	{
		"default_split_user_count",
		"Startup value of SPLITUSERS",
		INFO_DECIMAL(&ConfigChannel.default_split_user_count),
	},
	{
		"knock_delay",
		"Delay between a users KNOCK attempts",
		INFO_DECIMAL(&ConfigChannel.knock_delay),
	},
	{
		"knock_delay_channel",
		"Delay between KNOCK attempts to a channel",
		INFO_DECIMAL(&ConfigChannel.knock_delay_channel),
	},
	{
		"kick_on_split_riding",
		"Kick users riding splits to join +i or +k channels",
		INFO_INTBOOL_YN(&ConfigChannel.kick_on_split_riding),
	},
	{
		"disable_local_channels",
		"Disable local channels (&channels)",
		INFO_INTBOOL_YN(&ConfigChannel.disable_local_channels),
	},
	{
		"max_bans",
		"Total +b/e/I/q modes allowed in a channel",
		INFO_DECIMAL(&ConfigChannel.max_bans),
	},
	{
		"max_bans_large",
		"Total +b/e/I/q modes allowed in a +L channel",
		INFO_DECIMAL(&ConfigChannel.max_bans_large),
	},
	{
		"max_chans_per_user",
		"Maximum number of channels a user can join",
		INFO_DECIMAL(&ConfigChannel.max_chans_per_user),
	},
	{
		"max_chans_per_user_large",
		"Maximum extended number of channels a user can join",
		INFO_DECIMAL(&ConfigChannel.max_chans_per_user_large),
	},
	{
		"no_create_on_split",
		"Disallow creation of channels when split",
		INFO_INTBOOL_YN(&ConfigChannel.no_create_on_split),
	},
	{
		"no_join_on_split",
		"Disallow joining channels when split",
		INFO_INTBOOL_YN(&ConfigChannel.no_join_on_split),
	},
	{
		"only_ascii_channels",
		"Controls whether non-ASCII is disabled for JOIN",
		INFO_INTBOOL_YN(&ConfigChannel.only_ascii_channels),
	},
	{
		"use_except",
		"Enable chanmode +e (ban exceptions)",
		INFO_INTBOOL_YN(&ConfigChannel.use_except),
	},
	{
		"use_invex",
		"Enable chanmode +I (invite exceptions)",
		INFO_INTBOOL_YN(&ConfigChannel.use_invex),
	},
	{
		"use_forward",
		"Enable chanmode +f (channel forwarding)",
		INFO_INTBOOL_YN(&ConfigChannel.use_forward),
	},
	{
		"use_knock",
		"Enable /KNOCK",
		INFO_INTBOOL_YN(&ConfigChannel.use_knock),
	},
	{
		"resv_forcepart",
		"Force-part local users on channel RESV",
		INFO_INTBOOL_YN(&ConfigChannel.resv_forcepart),
	},
	{
		"opmod_send_statusmsg",
		"Send messages to @#channel if affected by +z",
		INFO_INTBOOL_YN(&ConfigChannel.opmod_send_statusmsg),
	},
	{
		"hide_opers",
		"Hide all opers from unprivileged users",
		INFO_INTBOOL_YN(&ConfigFileEntry.hide_opers),
	},
	{
		"hide_opers_in_whois",
		"Don't send RPL_WHOISOPERATOR to non-opers",
		INFO_INTBOOL_YN(&ConfigFileEntry.hide_opers_in_whois),
	},
	{
		"disable_hidden",
		"Prevent servers from hiding themselves from a flattened /links",
		INFO_INTBOOL_YN(&ConfigServerHide.disable_hidden),
	},
	{
		"flatten_links",
		"Flatten /links list",
		INFO_INTBOOL_YN(&ConfigServerHide.flatten_links),
	},
	{
		"hidden",
		"Hide this server from a flattened /links on remote servers",
		INFO_INTBOOL_YN(&ConfigServerHide.hidden),
	},
	{
		"links_delay",
		"Links rehash delay",
		INFO_DECIMAL(&ConfigServerHide.links_delay),
	},
	{
		"oper_secure_only",
		"Require TLS to become an oper",
		INFO_YESNOMASK(&ConfigFileEntry.oper_secure_only),
	},

	{ NULL, NULL, 0, { NULL } },
};
/* *INDENT-ON* */

/*
 ** m_info
 **  parv[1] = servername
 */
static void
m_info(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0L;

	if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
	{
		/* safe enough to give this on a local connect only */
		sendto_one(source_p, form_str(RPL_LOAD2HI),
				me.name, source_p->name, "INFO");
		sendto_one_numeric(source_p, RPL_ENDOFINFO, form_str(RPL_ENDOFINFO));
		return;
	}
	else
		last_used = rb_current_time();

	if(hunt_server(client_p, source_p, ":%s INFO :%s", 1, parc, parv) != HUNTED_ISME)
		return;

	info_spy(source_p);

	send_info_text(source_p);
	send_birthdate_online_time(source_p);

	sendto_one_numeric(source_p, RPL_ENDOFINFO, form_str(RPL_ENDOFINFO));
}

/*
 ** mo_info
 **  parv[1] = servername
 */
static void
mo_info(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(hunt_server(client_p, source_p, ":%s INFO :%s", 1, parc, parv) == HUNTED_ISME)
	{
		info_spy(source_p);
		send_info_text(source_p);

		if(IsOperGeneral(source_p))
		{
			send_conf_options(source_p);
			sendto_one_numeric(source_p, RPL_INFO, ":%s",
					rb_lib_version());
		}

		send_birthdate_online_time(source_p);

		sendto_one_numeric(source_p, RPL_ENDOFINFO, form_str(RPL_ENDOFINFO));
	}
}

/*
 * send_info_text
 *
 * inputs	- client pointer to send info text to
 * output	- none
 * side effects	- info text is sent to client
 */
static void
send_info_text(struct Client *source_p)
{
	const char **text = infotext;

	while (*text)
	{
		sendto_one_numeric(source_p, RPL_INFO, form_str(RPL_INFO), *text++);
	}

	sendto_one_numeric(source_p, RPL_INFO, form_str(RPL_INFO), "");
}

/*
 * send_birthdate_online_time
 *
 * inputs	- client pointer to send to
 * output	- none
 * side effects	- birthdate and online time are sent
 */
static void
send_birthdate_online_time(struct Client *source_p)
{
	char tbuf[26]; /* this needs to be 26 - see ctime_r manpage */
	sendto_one(source_p, ":%s %d %s :Birth Date: %s, compile # %s",
			get_id(&me, source_p), RPL_INFO,
			get_id(source_p, source_p), creation, generation);

	sendto_one(source_p, ":%s %d %s :On-line since %s",
			get_id(&me, source_p), RPL_INFO,
			get_id(source_p, source_p), rb_ctime(startup_time, tbuf, sizeof(tbuf)));
}

/*
 * send_conf_options
 *
 * inputs	- client pointer to send to
 * output	- none
 * side effects	- send config options to client
 */
static void
send_conf_options(struct Client *source_p)
{
	Info *infoptr;
	int i = 0;

	/*
	 * Now send them a list of all our configuration options
	 * (mostly from defaults.h)
	 */
	for (infoptr = MyInformation; infoptr->name; infoptr++)
	{
		if(infoptr->intvalue)
		{
			sendto_one(source_p, ":%s %d %s :%-30s %-16d [%s]",
					get_id(&me, source_p), RPL_INFO,
					get_id(source_p, source_p),
					infoptr->name, infoptr->intvalue,
					infoptr->desc);
		}
		else
		{
			sendto_one(source_p, ":%s %d %s :%-30s %-16s [%s]",
					get_id(&me, source_p), RPL_INFO,
					get_id(source_p, source_p),
					infoptr->name, infoptr->strvalue,
					infoptr->desc);
		}
	}

	/*
	 * Parse the info_table[] and do the magic.
	 */
	for (i = 0; info_table[i].name; i++)
	{
		static char opt_buf[BUFSIZE];
		const char *opt_value = opt_buf;


		switch (info_table[i].output_type)
		{
		case OUTPUT_STRING:
		{
			const char *option = *info_table[i].option.string_p;
			opt_value = option != NULL ? option : "NONE";
			break;
		}
		case OUTPUT_STRING_PTR:
		{
			const char *option = info_table[i].option.string;
			opt_value = option != NULL ? option : "NONE";
			break;
		}
		case OUTPUT_DECIMAL:
		{
			int option = *info_table[i].option.int_;
			snprintf(opt_buf, sizeof opt_buf, "%d", option);
			break;
		}
		case OUTPUT_BOOLEAN:
		{
			bool option = *info_table[i].option.bool_;
			opt_value = option ? "ON" : "OFF";
			break;
		}
		case OUTPUT_BOOLEAN_YN:
		{
			bool option = *info_table[i].option.bool_;
			opt_value = option ? "YES" : "NO";
			break;
		}
		case OUTPUT_YESNOMASK:
		{
			int option = *info_table[i].option.int_;
			opt_value = option == 0 ? "NO" :
			            option == 1 ? "MASK" :
			            "YES";
			break;
		}
		case OUTPUT_INTBOOL:
		{
			bool option = *info_table[i].option.int_;
			opt_value = option ? "ON" : "OFF";
			break;
		}
		case OUTPUT_INTBOOL_YN:
		{
			bool option = *info_table[i].option.int_;
			opt_value = option ? "YES" : "NO";
			break;
		}
		case OUTPUT_STATSL:
		{
			enum stats_l_oper_only option = *info_table[i].option.statsl;
			opt_value = option == STATS_L_OPER_ONLY_NO ? "NO" :
			            option == STATS_L_OPER_ONLY_SELF ? "SELF" :
			            "YES";
			break;
		}
		}

		sendto_one(source_p, ":%s %d %s :%-30s %-16s [%s]",
				get_id(&me, source_p), RPL_INFO,
				get_id(source_p, source_p),
				info_table[i].name,
				opt_value,
				info_table[i].desc ? info_table[i].desc : "<none>");
	}


	/* Don't send oper_only_umodes...it's a bit mask, we will have to decode it
	 ** in order for it to show up properly to opers who issue INFO
	 */

	sendto_one_numeric(source_p, RPL_INFO, form_str(RPL_INFO), "");
}

/* info_spy()
 *
 * input        - pointer to client
 * output       - none
 * side effects - hook doing_info is called
 */
static void
info_spy(struct Client *source_p)
{
	hook_data hd;

	hd.client = source_p;
	hd.arg1 = hd.arg2 = NULL;

	call_hook(doing_info_hook, &hd);
}
