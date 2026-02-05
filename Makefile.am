AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = librb

if BUILD_LTDL
SUBDIRS += libltdl
endif

SUBDIRS += ircd \
           ssld \
           authd \
           bandb \
           tests \
           tools \
           modules \
           extensions \
           help \
           doc

BUILT_SOURCES = include/serno.h

include/serno.h:
	@sh include/serno.h.SH > $@

install-data-hook:
	test -d ${DESTDIR}${logdir} || mkdir -p ${DESTDIR}${logdir}
	@# needed for FHS paths:
	test -d ${DESTDIR}${pkglocalstatedir} || mkdir -p ${DESTDIR}${pkglocalstatedir}

install-exec-hook:
	rm -f ${DESTDIR}${libdir}/*.la
	rm -f ${DESTDIR}${moduledir}/*.la
	rm -f ${DESTDIR}${moduledir}/autoload/*.la
	rm -f ${DESTDIR}${moduledir}/extensions/*.la
	rm -f ${DESTDIR}${libdir}/*.dll.a
	rm -f ${DESTDIR}${moduledir}/*.dll.a
	rm -f ${DESTDIR}${moduledir}/autoload/*.dll.a
	rm -f ${DESTDIR}${moduledir}/extensions/*.dll.a

distclean-local:
	rm -f librb/include/librb-config.h

clean-local:
	rm -f include/serno.h
	rm -f ircd/ircd_lexer.c
	rm -f ircd/ircd_parser.c
	rm -f ircd/ircd_parser.h
	rm -f ircd/version.c
	rm -f ircd/version.c.last
