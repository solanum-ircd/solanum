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

#include "rb_lib.h"
#include "ircd_defs.h"  /* Needed for some reasons here -- dwr */

/* AIX requires this to be the first thing in the file.  */
#ifdef __GNUC__
#undef alloca
#define alloca __builtin_alloca
#else
# ifdef _MSC_VER
#  include <malloc.h>
#  define alloca _alloca
# else
#  if HAVE_ALLOCA_H
#   include <alloca.h>
#  else
#   ifdef _AIX
 #pragma alloca
#   else
#    ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#    endif
#   endif
#  endif
# endif
#endif


#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef STRING_WITH_STRINGS
# include <string.h>
# include <strings.h>
#else
# ifdef HAVE_STRING_H
#  include <string.h>
# else
#  ifdef HAVE_STRINGS_H
#   include <strings.h>
#  endif
# endif
#endif

#include <stddef.h>

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif


#include <stdio.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <stdarg.h>
#include <signal.h>
#include <dirent.h>
#include <ctype.h>

#include <limits.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/resource.h>

#include <sys/stat.h>

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#include <errno.h>
#include <sys/uio.h>

#if defined(__INTEL_COMPILER) || defined(__GNUC__)
# ifdef __unused
#  undef __unused
# endif
# ifdef __printf
#  undef __printf
# endif
# ifdef __noreturn
#  undef __noreturn
# endif

# define __unused __attribute__((__unused__))
# define __printf(x) __attribute__((__format__ (__printf__, x, x + 1)))
# define __noreturn __attribute__((__noreturn__))
#else
# define __unused
# define __printf
# define __noreturn
#endif



#ifdef strdupa
#define LOCAL_COPY(s) strdupa(s)
#else
#if defined(__INTEL_COMPILER) || defined(__GNUC__)
# define LOCAL_COPY(s) __extension__({ char *_s = alloca(strlen(s) + 1); strcpy(_s, s); _s; })
#else
# define LOCAL_COPY(s) strcpy(alloca(strlen(s) + 1), s) /* XXX Is that allowed? */
#endif /* defined(__INTEL_COMPILER) || defined(__GNUC__) */
#endif /* strdupa */
