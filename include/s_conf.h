/*
 *  solanum: Advanced, scalable Internet Relay Chat.
 *  s_conf.h: A header for the configuration functions.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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

#ifndef INCLUDED_s_conf_h
#define INCLUDED_s_conf_h
#include "setup.h"

#ifdef HAVE_LIBCRYPTO
#include <openssl/rsa.h>
#endif

#include "ircd_defs.h"
#include "class.h"
#include "client.h"

struct Client;
struct DNSReply;
struct hostent;

/* used by new parser */
/* yacc/lex love globals!!! */

struct ip_value
{
	struct rb_sockaddr_storage ip;
	int ip_mask;
	int type;
};

extern FILE *conf_fbfile_in;
extern char conf_line_in[256];

struct ConfItem
{
	unsigned int status;	/* If CONF_ILLEGAL, delete when no clients */
	unsigned int flags;
	int clients;		/* Number of *LOCAL* clients using this */
	union
	{
		char *name;	/* IRC name, nick, server name, or original u@h */
		const char *oper;
	} info;
	char *host;		/* host part of user@host */
	char *passwd;		/* doubles as kline reason *ugh* */
	char *spasswd;		/* Password to send. */
	char *user;		/* user part of user@host */
	char *desc;             /* description */
	int port;
	time_t hold;		/* Hold action until this time (calendar time) */
	time_t created;		/* Creation time (for klines etc) */
	time_t lifetime;	/* Propagated lines: remember until this time */
	char *className;	/* Name of class */
	struct Class *c_class;	/* Class of connection */
	rb_patricia_node_t *pnode;	/* Our patricia node */
	int umodes, umodes_mask;	/* Override umodes specified by mask */
};

#define CONF_ILLEGAL		0x80000000
#define CONF_CLIENT		0x0002
#define CONF_KILL		0x0040
#define CONF_XLINE		0x0080
#define CONF_RESV_CHANNEL	0x0100
#define CONF_RESV_NICK		0x0200
#define CONF_RESV		(CONF_RESV_CHANNEL | CONF_RESV_NICK)

#define CONF_DLINE		0x020000
#define CONF_EXEMPTDLINE	0x100000
#define CONF_SECURE		0x200000

#define IsIllegal(x)    ((x)->status & CONF_ILLEGAL)

/* aConfItem->flags */

/* Generic flags... */
#define CONF_FLAGS_TEMPORARY		0x00800000
#define CONF_FLAGS_NEED_SSL		0x00000002
#define CONF_FLAGS_MYOPER		0x00080000	/* need to rewrite info.oper on burst */
/* auth{} flags... */
#define CONF_FLAGS_NO_TILDE		0x00000004
#define CONF_FLAGS_NEED_IDENTD		0x00000008
#define CONF_FLAGS_EXEMPTKLINE		0x00000040
#define CONF_FLAGS_NOLIMIT		0x00000080
#define CONF_FLAGS_SPOOF_IP		0x00000200
#define CONF_FLAGS_SPOOF_NOTICE		0x00000400
#define CONF_FLAGS_REDIR		0x00000800
#define CONF_FLAGS_EXEMPTRESV		0x00002000	/* exempt from resvs */
#define CONF_FLAGS_EXEMPTFLOOD		0x00004000
#define CONF_FLAGS_EXEMPTSPAMBOT	0x00008000
#define CONF_FLAGS_EXEMPTSHIDE		0x00010000
#define CONF_FLAGS_EXEMPTJUPE		0x00020000	/* exempt from resv generating warnings */
#define CONF_FLAGS_NEED_SASL		0x00040000
#define CONF_FLAGS_EXTEND_CHANS		0x00080000
#define CONF_FLAGS_ENCRYPTED		0x00200000
#define CONF_FLAGS_EXEMPTDNSBL		0x04000000
#define CONF_FLAGS_EXEMPTPROXY		0x08000000
#define CONF_FLAGS_ALLOW_SCTP		0x10000000
#define CONF_FLAGS_KLINE_SPOOF		0x20000000


/* Macros for struct ConfItem */
#define IsConfBan(x)		((x)->status & (CONF_KILL|CONF_XLINE|CONF_DLINE|\
						CONF_RESV_CHANNEL|CONF_RESV_NICK))

#define IsNoTilde(x)            ((x)->flags & CONF_FLAGS_NO_TILDE)
#define IsNeedIdentd(x)         ((x)->flags & CONF_FLAGS_NEED_IDENTD)
#define IsConfExemptKline(x)    ((x)->flags & CONF_FLAGS_EXEMPTKLINE)
#define IsConfExemptLimits(x)   ((x)->flags & CONF_FLAGS_NOLIMIT)
#define IsConfExemptFlood(x)    ((x)->flags & CONF_FLAGS_EXEMPTFLOOD)
#define IsConfExemptSpambot(x)	((x)->flags & CONF_FLAGS_EXEMPTSPAMBOT)
#define IsConfExemptShide(x)	((x)->flags & CONF_FLAGS_EXEMPTSHIDE)
#define IsConfExemptJupe(x)	((x)->flags & CONF_FLAGS_EXEMPTJUPE)
#define IsConfExemptResv(x)	((x)->flags & CONF_FLAGS_EXEMPTRESV)
#define IsConfDoSpoofIp(x)      ((x)->flags & CONF_FLAGS_SPOOF_IP)
#define IsConfSpoofNotice(x)    ((x)->flags & CONF_FLAGS_SPOOF_NOTICE)
#define IsConfEncrypted(x)      ((x)->flags & CONF_FLAGS_ENCRYPTED)
#define IsNeedSasl(x)		((x)->flags & CONF_FLAGS_NEED_SASL)
#define IsConfExemptDNSBL(x)	((x)->flags & CONF_FLAGS_EXEMPTDNSBL)
#define IsConfExemptProxy(x)	((x)->flags & CONF_FLAGS_EXEMPTPROXY)
#define IsConfExtendChans(x)	((x)->flags & CONF_FLAGS_EXTEND_CHANS)
#define IsConfSSLNeeded(x)	((x)->flags & CONF_FLAGS_NEED_SSL)
#define IsConfAllowSCTP(x)	((x)->flags & CONF_FLAGS_ALLOW_SCTP)
#define IsConfKlineSpoof(x)	((x)->flags & CONF_FLAGS_KLINE_SPOOF)

enum stats_l_oper_only {
	STATS_L_OPER_ONLY_NO,
	STATS_L_OPER_ONLY_SELF,
	STATS_L_OPER_ONLY_YES,
};

/* flag definitions for opers now in client.h */

struct config_file_entry
{
	const char *dpath;	/* DPATH if set from command line */
	const char *configfile;

	char *default_operstring;
	char *default_adminstring;
	char *servicestring;
	char *kline_reason;

	char *identifyservice;
	char *identifycommand;

	char *sasl_service;

	char *fname_userlog;
	char *fname_fuserlog;
	char *fname_operlog;
	char *fname_foperlog;
	char *fname_serverlog;
	char *fname_killlog;
	char *fname_klinelog;
	char *fname_operspylog;
	char *fname_ioerrorlog;

	int disable_fake_channels;
	int dots_in_ident;
	int failed_oper_notice;
	int anti_nick_flood;
	int anti_spam_exit_message_time;
	int max_accept;
	int max_monitor;
	int max_nick_time;
	int max_nick_changes;
	int ts_max_delta;
	int ts_warn_delta;
	int dline_with_reason;
	int kline_with_reason;
	int hide_tkdline_duration;
	int warn_no_nline;
	int nick_delay;
	int non_redundant_klines;
	int stats_e_disabled;
	int stats_c_oper_only;
	int stats_y_oper_only;
	int stats_o_oper_only;
	int stats_k_oper_only;
	enum stats_l_oper_only stats_l_oper_only;
	int stats_i_oper_only;
	int stats_P_oper_only;
	int map_oper_only;
	int operspy_admin_only;
	int pace_wait;
	int pace_wait_simple;
	int ping_warn_time;
	int short_motd;
	int no_oper_flood;
	int hide_server;
	int hide_spoof_ips;
	int hide_error_messages;
	int client_exit;
	int oper_only_umodes;
	int oper_umodes;
	int oper_snomask;
	int max_targets;
	int caller_id_wait;
	int min_nonwildcard;
	int min_nonwildcard_simple;
	int default_floodcount;
	int default_ident_timeout;
	int ping_cookie;
	int tkline_expire_notices;
	int use_whois_actually;
	int disable_auth;
	int post_registration_delay;
	int connect_timeout;
	int burst_away;
	int reject_ban_time;
	int reject_after_count;
	int reject_duration;
	int throttle_count;
	int throttle_duration;
	int target_change;
	int collision_fnc;
	int resv_fnc;
	int default_umodes;
	int global_snotices;
	int operspy_dont_care_user_info;
	int use_propagated_bans;
	int max_ratelimit_tokens;
	int away_interval;
	int tls_ciphers_oper_only;
	int oper_secure_only;

	char **hidden_caps;

	int client_flood_max_lines;
	int client_flood_burst_rate;
	int client_flood_burst_max;
	int client_flood_message_time;
	int client_flood_message_num;

	unsigned int nicklen;
	int certfp_method;

	int hide_opers_in_whois;
	int hide_opers;

	char *drain_reason;
	char *sasl_only_client_message;
	char *identd_only_client_message;
	char *sctp_forbidden_client_message;
	char *ssltls_only_client_message;
	char *not_authorised_client_message;
	char *illegal_hostname_client_message;
	char *server_full_client_message;
	char *illegal_name_long_client_message;
	char *illegal_name_short_client_message;
};

struct config_channel_entry
{
	int use_except;
	int use_invex;
	int use_forward;
	int use_knock;
	int knock_delay;
	int knock_delay_channel;
	int max_bans;
	int max_bans_large;
	int max_chans_per_user;
	int max_chans_per_user_large;
	int no_create_on_split;
	int no_join_on_split;
	int default_split_server_count;
	int default_split_user_count;
	int burst_topicwho;
	int kick_on_split_riding;
	int only_ascii_channels;
	int resv_forcepart;
	int channel_target_change;
	int disable_local_channels;
	unsigned int autochanmodes;
	int displayed_usercount;
	int strip_topic_colors;
	int opmod_send_statusmsg;
	int ip_bans_through_vhost;
	int invite_notify_notice;
};

struct config_server_hide
{
	int flatten_links;
	int links_delay;
	int hidden;
	int disable_hidden;
};

struct server_info
{
	char *name;
	char sid[4];
	char *description;
	char *network_name;
	struct rb_sockaddr_storage bind4;
	struct rb_sockaddr_storage bind6;
	int default_max_clients;
	char *ssl_private_key;
	char *ssl_ca_cert;
	char *ssl_cert;
	char *ssl_dh_params;
	char *ssl_cipher_list;
	int ssld_count;
};

struct admin_info
{
	char *name;
	char *description;
	char *email;
};

struct alias_entry
{
	char *name;
	char *target;
	int flags;			/* reserved for later use */
};

/* All variables are GLOBAL */
extern struct config_file_entry ConfigFileEntry;	/* defined in ircd.c */
extern struct config_channel_entry ConfigChannel;	/* defined in channel.c */
extern struct config_server_hide ConfigServerHide;	/* defined in s_conf.c */
extern struct server_info ServerInfo;	/* defined in ircd.c */
extern struct admin_info AdminInfo;	/* defined in ircd.c */
/* End GLOBAL section */

extern rb_dlink_list service_list;

extern rb_dictionary *prop_bans_dict;

typedef enum temp_list
{
	TEMP_MIN,
	TEMP_HOUR,
	TEMP_DAY,
	TEMP_WEEK,
	LAST_TEMP_TYPE
} temp_list;

extern rb_dlink_list temp_klines[LAST_TEMP_TYPE];
extern rb_dlink_list temp_dlines[LAST_TEMP_TYPE];

extern void init_s_conf(void);

extern struct ConfItem *make_conf(void);
extern void free_conf(struct ConfItem *);

extern struct ConfItem *find_prop_ban(unsigned int status, const char *user, const char *host);
extern void add_prop_ban(struct ConfItem *);
extern void remove_prop_ban(struct ConfItem *);
extern bool lookup_prop_ban(struct ConfItem *);
extern void deactivate_conf(struct ConfItem *, time_t);
extern void replace_old_ban(struct ConfItem *);

extern void read_conf_files(bool cold);

extern int attach_conf(struct Client *, struct ConfItem *);
extern int check_client(struct Client *client_p, struct Client *source_p, const char *);

extern void deref_conf(struct ConfItem *);
extern int detach_conf(struct Client *);

extern struct ConfItem *find_tkline(const char *, const char *, struct sockaddr *);
extern char *show_iline_prefix(struct Client *, struct ConfItem *, char *);
extern void get_printable_conf(struct ConfItem *,
			       char **, char **, const char **, char **, int *, char **, char **);
extern char *get_user_ban_reason(struct ConfItem *aconf);
extern void get_printable_kline(struct Client *, struct ConfItem *,
				char **, char **, char **, char **);

extern void yyerror(const char *);
extern int conf_yy_fatal_error(const char *);
extern int conf_fgets(char *, int, FILE *);

extern int valid_wild_card(const char *, const char *);
extern void add_temp_kline(struct ConfItem *);
extern void add_temp_dline(struct ConfItem *);
extern void report_temp_klines(struct Client *);
extern void show_temp_klines(struct Client *, rb_dlink_list *);

extern bool rehash(bool);
extern void rehash_bans(void);

extern int conf_add_server(struct ConfItem *, int);
extern void conf_add_class_to_conf(struct ConfItem *);
extern void conf_add_me(struct ConfItem *);
extern void conf_add_class(struct ConfItem *, int);
extern void conf_add_d_conf(struct ConfItem *);
extern void flush_expired_ips(void *);

extern const char *get_oper_name(struct Client *client_p);

extern int yylex(void);

extern unsigned long cidr_to_bitmask[];

extern char conffilebuf[BUFSIZE + 1];
extern int lineno;

#define NOT_AUTHORISED  (-1)
#define I_SOCKET_ERROR  (-2)
#define I_LINE_FULL     (-3)
#define BANNED_CLIENT   (-4)
#define TOO_MANY_LOCAL	(-6)
#define TOO_MANY_GLOBAL (-7)
#define TOO_MANY_IDENT	(-8)

#endif /* INCLUDED_s_conf_h */
