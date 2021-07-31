pkglibexec_PROGRAMS = authd
AM_CFLAGS=$(WARNFLAGS)
AM_CPPFLAGS = -I../include -I../librb/include 

authd_SOURCES =	\
	authd.c	\
	dns.c \
	notice.c \
	provider.c \
	res.c \
	reslib.c \
	providers/dnsbl.c \
	providers/ident.c \
	providers/rdns.c \
	providers/opm.c

authd_LDADD = ../librb/src/librb.la
