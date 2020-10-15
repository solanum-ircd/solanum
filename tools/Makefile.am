bin_PROGRAMS = solanum-mkpasswd solanum-mkfingerprint
AM_CFLAGS=$(WARNFLAGS)
AM_CPPFLAGS = $(DEFAULT_INCLUDES) -I../librb/include -I.

solanum_mkpasswd_SOURCES = mkpasswd.c
solanum_mkpasswd_LDADD = ../librb/src/librb.la

solanum_mkfingerprint_SOURCES = mkfingerprint.c
solanum_mkfingerprint_LDADD = ../librb/src/librb.la
