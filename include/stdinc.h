/*
 *  ircd-ratbox: A slightly useful ircd.
 *  stdinc.h: Pull in all of the necessary system headers
 *
 *  Copyright (C) 2002 Aaron Sethman <androsyn@ratbox.org>
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
 *
 */

#ifndef INCLUDED_stdinc_h
#define INCLUDED_stdinc_h 1

#include "rb_lib.h"
#include "ircd_defs.h"  /* Needed for some reasons here -- dwr */

#include <stdbool.h>
#include <stdlib.h>

#include <dirent.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif
#include <unistd.h>

#include <sys/file.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#if defined(strdupa)
#  define LOCAL_COPY(s)         strdupa(s)
#elif defined(__GNUC__) || defined(__INTEL_COMPILER)
#  define LOCAL_COPY(s)         __extension__({ char *_s = alloca(strlen(s) + 1); strcpy(_s, s); _s; })
#else
#  define LOCAL_COPY(s)         strcpy(alloca(strlen(s) + 1), s) /* XXX Is that allowed? */
#endif

#endif /* !INCLUDED_stdinc_h */
