check_PROGRAMS = runtests \
	chmode1 \
	match1 \
	msgbuf_parse1 \
	msgbuf_unparse1 \
	hostmask1 \
	rb_dictionary1 \
	rb_snprintf_append1 \
	rb_snprintf_try_append1 \
	sasl_abort1 \
	send1 \
	serv_connect1 \
	substitution1
AM_CFLAGS=$(WARNFLAGS)
AM_CPPFLAGS = $(DEFAULT_INCLUDES) -I../librb/include -I..
AM_LDFLAGS = -no-install
LDADD = tap/libtap.a ../librb/src/librb.la ../ircd/libircd.la -ldl

# Override -rpath or programs will be linked to installed libraries
libdir=$(abs_top_builddir)

runtests_CPPFLAGS = -DC_TAP_SOURCE='"$(abs_top_srcdir)/tests"' \
	-DC_TAP_BUILD='"$(abs_top_builddir)/tests"'
check_LIBRARIES = tap/libtap.a
tap_libtap_a_SOURCES = tap/basic.c tap/basic.h \
	tap/float.c tap/float.h tap/macros.h

chmode1_SOURCES = chmode1.c ircd_util.c client_util.c
match1_SOURCES = match1.c
msgbuf_parse1_SOURCES = msgbuf_parse1.c
msgbuf_unparse1_SOURCES = msgbuf_unparse1.c
hostmask1_SOURCES = hostmask1.c
rb_dictionary1_SOURCES = rb_dictionary1.c
rb_snprintf_append1_SOURCES = rb_snprintf_append1.c
rb_snprintf_try_append1_SOURCES = rb_snprintf_try_append1.c
sasl_abort1_SOURCES = sasl_abort1.c ircd_util.c client_util.c
send1_SOURCES = send1.c ircd_util.c client_util.c
serv_connect1_SOURCES = serv_connect1.c ircd_util.c client_util.c
substitution1_SOURCES = substitution1.c

check-local: $(check_PROGRAMS) \
	../authd/authd \
	../bandb/bandb \
	../ssld/ssld \
	../wsockd/wsockd \
	$(patsubst ../modules/%.c,../modules/.libs/%.so,$(wildcard ../modules/*.c)) \
	$(patsubst ../modules/core/%.c,../modules/core/.libs/%.so,$(wildcard ../modules/core/*.c))

	ASAN_OPTIONS="${ASAN_OPTIONS}:detect_leaks=false" ./runtests -l $(abs_top_srcdir)/tests/TESTS
