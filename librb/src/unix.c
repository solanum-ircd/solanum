/*
 *  ircd-ratbox: A slightly useful ircd.
 *  unix.c: various unix type functions
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 2005 ircd-ratbox development team
 *  Copyright (C) 2005 Aaron Sethman <androsyn@ratbox.org>
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
 *
 */

#include <librb_config.h>
#include <rb_lib.h>
#include <sys/wait.h>

#ifdef HAVE_DLINFO
# include <link.h>
# include <dlfcn.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <crt_externs.h>
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include <spawn.h>

#ifndef __APPLE__
extern char **environ;
#endif
pid_t
rb_spawn_process(const char *path, const char **argv)
{
	pid_t pid;
	const void *arghack = argv;
	char **myenviron;
	int error;
	posix_spawnattr_t spattr;
	posix_spawnattr_init(&spattr);
#ifdef POSIX_SPAWN_USEVFORK
	posix_spawnattr_setflags(&spattr, POSIX_SPAWN_USEVFORK);
#endif
#ifdef __APPLE__
 	myenviron = *_NSGetEnviron(); /* apple needs to go fuck themselves for this */
#else
	myenviron = environ;
#endif
	error = posix_spawn(&pid, path, NULL, &spattr, arghack, myenviron);
	posix_spawnattr_destroy(&spattr);
	if (error != 0)
	{
		errno = error;
		pid = -1;
	}
	return pid;
}

int
rb_gettimeofday(struct timeval *tv, void *tz)
{
	return (gettimeofday(tv, tz));
}

void
rb_sleep(unsigned int seconds, unsigned int useconds)
{
	struct timespec tv;
	tv.tv_nsec = (useconds * 1000);
	tv.tv_sec = seconds;
	nanosleep(&tv, NULL);
}

int
rb_kill(pid_t pid, int sig)
{
	return kill(pid, sig);
}

int
rb_setenv(const char *name, const char *value, int overwrite)
{
	return setenv(name, value, overwrite);
}
