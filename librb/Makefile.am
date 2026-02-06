AUTOMAKE_OPTIONS = foreign

SUBDIRS = src

BUILT_SOURCES = include/serno.h include/datecode.h

# NOTE: you'll want to change the below if you are unbundling
include/serno.h:
	@if [ -f ../include/serno.h ]; then \
		cp ../include/serno.h include/; \
	else \
		echo '#define SERNO "unknown"' >include/serno.h; \
	fi

include/datecode.h:
	@if [ -f ../include/datecode.h ]; then \
		cp ../include/datecode.h include/; \
	else \
		echo '#define DATECODE 0UL' >include/datecode.h; \
	fi

clean-local:
	rm -f include/serno.h
	rm -f include/datecode.h
	rm -f src/version.c
	rm -f src/version.c.last
