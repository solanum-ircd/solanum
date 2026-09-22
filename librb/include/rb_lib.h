#ifndef RB_LIB_H
#define RB_LIB_H 1

#include <librb-config.h>

#include <stddef.h>
#include <sys/types.h>

#if defined(HAVE_ALLOCA_H)
#  include <alloca.h>
#elif !defined(alloca)
#  if defined(__GNUC__)
#    define alloca              __builtin_alloca
#  elif defined(_AIX)
#    define alloca              __alloca
#  elif !defined(HAVE_ALLOCA)
void *alloca(size_t);
#  endif
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <time.h>

#ifdef rb_likely
#  undef rb_likely
#endif
#ifdef rb_unlikely
#  undef rb_unlikely
#endif
#ifdef __GNUC__
#  define rb_likely(x)          __builtin_expect(!!(x), 1)
#  define rb_unlikely(x)        __builtin_expect(!!(x), 0)
#else
#  define rb_likely(x)          (x)
#  define rb_unlikely(x)        (x)
#endif


/* For those unfamiliar with GNU format attributes, a is the 1 based
 * argument number of the format string, and b is the 1 based argument
 * number of the variadic ... */
#if __has_attribute(__format__)
#  define AFP(a, b)             __attribute__((__format__(printf, a, b)))
#else
#  define AFP(a, b)
#endif

#if __has_attribute(__noreturn__)
#  define __noreturn            __attribute__((__noreturn__))
#else
#  define __noreturn
#endif

#if __has_attribute(__unused__)
#  define __unused              __attribute__((__unused__))
#else
#  define __unused
#endif


#define slrb_assert(expr)	(rb_likely((expr)) || (rb_lib_log(	\
	"file: %s line: %d (%s): Assertion failed: (%s)",	\
	__FILE__, __LINE__, __func__, #expr), 0))

#define lrb_assert(expr)	assert(slrb_assert(expr))

#ifdef RB_SOCKADDR_HAS_SA_LEN
#  define ss_len sa_len
#endif

#define GET_SS_FAMILY(x) (((const struct sockaddr *)(x))->sa_family)
#define SET_SS_FAMILY(x, y) ((((struct sockaddr *)(x))->sa_family) = y)

#ifdef RB_SOCKADDR_HAS_SA_LEN
#  define SET_SS_LEN(x, y)	do {							\
					struct sockaddr *_storage;		\
					_storage = ((struct sockaddr *)(x));\
					_storage->sa_len = (y);				\
				} while (0)
#  define GET_SS_LEN(x) (((struct sockaddr *)(x))->sa_len)
#else /* !RB_SOCKADDR_HAS_SA_LEN */
#  define SET_SS_LEN(x, y) (((struct sockaddr *)(x))->sa_family = ((struct sockaddr *)(x))->sa_family)
#  define GET_SS_LEN(x) (((struct sockaddr *)(x))->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6))
#endif

#define GET_SS_PORT(x) (((struct sockaddr *)(x))->sa_family == AF_INET ? ((struct sockaddr_in *)(x))->sin_port : ((struct sockaddr_in6 *)(x))->sin6_port)
#define SET_SS_PORT(x, y)	do { \
					if(((struct sockaddr *)(x))->sa_family == AF_INET) { \
						((struct sockaddr_in *)(x))->sin_port = (y); \
					} else { \
						((struct sockaddr_in6 *)(x))->sin6_port = (y); \
					} \
				} while (0)

#ifndef HOSTIPLEN
#  define HOSTIPLEN     53
#endif

#ifndef INADDRSZ
#  define INADDRSZ      4
#endif

#ifndef IN6ADDRSZ
#  define IN6ADDRSZ     16
#endif

#ifndef INT16SZ
#  define INT16SZ       2
#endif


typedef void log_cb(const char *buffer);
typedef void restart_cb(const char *buffer);
typedef void die_cb(const char *buffer);

char *rb_ctime(const time_t, char *, size_t);
char *rb_date(const time_t, char *, size_t);
void rb_lib_log(const char *, ...);
void rb_lib_restart(const char *, ...) __noreturn;
void rb_lib_die(const char *, ...);
void rb_set_time(void);
const char *rb_lib_version(void);

void rb_lib_init(log_cb * xilog, restart_cb * irestart, die_cb * idie, int closeall, int maxfds,
		 size_t dh_size, size_t fd_heap_size);
void rb_lib_loop(long delay) __noreturn;

time_t rb_current_time(void);
const struct timeval *rb_current_time_tv(void);
pid_t rb_spawn_process(const char *, const char **);

char *rb_strtok_r(char *, const char *, char **);

int rb_gettimeofday(struct timeval *, void *);

void rb_sleep(unsigned int seconds, unsigned int useconds);
char *rb_crypt(const char *, const char *);

unsigned char *rb_base64_encode(const unsigned char *str, int length);
unsigned char *rb_base64_decode(const unsigned char *str, size_t length, int *ret);
int rb_kill(pid_t, int);
char *rb_strerror(int);

int rb_setenv(const char *, const char *, int);

pid_t rb_waitpid(pid_t pid, int *status, int options);
pid_t rb_getpid(void);


#include <rb_tools.h>
#include <rb_memory.h>
#include <rb_commio.h>
#include <rb_balloc.h>
#include <rb_linebuf.h>
#include <rb_event.h>
#include <rb_helper.h>
#include <rb_rawbuf.h>
#include <rb_patricia.h>

#endif
