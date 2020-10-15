/*
 *  ircd_util.c: Utility functions for making test ircds
 *  Copyright 2017 Simon Arlott
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "tap/basic.h"

#include "ircd_util.h"

#include "defaults.h"
#include "client.h"
#include "modules.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__

extern int solanum_main(int argc, const char *argv[]);

static char argv0[BUFSIZE];
static char configfile[BUFSIZE];
static char logfile[BUFSIZE];
static char pidfile[BUFSIZE];

static const char *argv[] = {
	argv0,
	"-configfile", configfile,
	"-logfile", logfile,
	"-pidfile", pidfile,
	"-conftest",
	NULL,
};

void ircd_util_init(const char *name)
{
	rb_strlcpy(argv0, name, sizeof(argv0));
	snprintf(configfile, sizeof(configfile), "%sonf", name);
	snprintf(logfile, sizeof(logfile), "%s.log", name);
	snprintf(pidfile, sizeof(pidfile), "%s.pid", name);
	unlink(logfile);
	unlink(pidfile);

	rb_lib_init(NULL, NULL, NULL, 0, 1024, DNODE_HEAP_SIZE, FD_HEAP_SIZE);
	rb_linebuf_init(LINEBUF_HEAP_SIZE);

	char buf[BUFSIZE];
	ircd_paths[IRCD_PATH_IRCD_EXEC] = argv0;
	ircd_paths[IRCD_PATH_PREFIX] = ".";

	snprintf(buf, sizeof(buf), "runtime%cmodules", RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_MODULES] = rb_strdup(buf);

	snprintf(buf, sizeof(buf), "runtime%1$cmodules%1$cautoload", RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_AUTOLOAD_MODULES] = rb_strdup(buf);

	ircd_paths[IRCD_PATH_ETC] = "runtime";
	ircd_paths[IRCD_PATH_LOG] = "runtime";

	snprintf(buf, sizeof(buf), "runtime%1$chelp%1$cusers", RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_USERHELP] = rb_strdup(buf);

	snprintf(buf, sizeof(buf), "runtime%1$chelp%1$copers", RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_OPERHELP] = rb_strdup(buf);

	snprintf(buf, sizeof(buf), "runtime%cmotd", RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_IRCD_MOTD] = rb_strdup(buf);
	ircd_paths[IRCD_PATH_IRCD_OMOTD] = rb_strdup(buf);

	snprintf(buf, sizeof(buf), "%s.ban.db", name);
	ircd_paths[IRCD_PATH_BANDB] = rb_strdup(buf);
	snprintf(buf, sizeof(buf), "%s.ban.db-journal", name);
	unlink(buf);

	ircd_paths[IRCD_PATH_IRCD_PID] = rb_strdup(pidfile);
	ircd_paths[IRCD_PATH_IRCD_LOG] = rb_strdup(logfile);

	snprintf(buf, sizeof(buf), "runtime%cbin", RB_PATH_SEPARATOR);
	ircd_paths[IRCD_PATH_BIN] = rb_strdup(buf);
	ircd_paths[IRCD_PATH_LIBEXEC] = rb_strdup(buf);

	is_int(0, solanum_main(ARRAY_SIZE(argv) - 1, argv), MSG);
}

void ircd_util_reload_module(const char *name)
{
	struct module *mod = findmodule_byname(name);
	int origin, core;

	if (ok(mod != NULL, MSG)) {
		origin = mod->origin;
		core = mod->core;
		if (ok(unload_one_module(name, false), MSG)) {
			ok(load_one_module(name, origin, core), MSG);
		}
	}
}

void ircd_util_free(void)
{
}
