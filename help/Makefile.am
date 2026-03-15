# Generated automatically from Makefile.in by configure.
# makefile for help/

AUTOMAKE_OPTIONS = foreign

prefix=		@prefix@
exec_prefix=	@execprefix@
helpdir=	@helpdir@
uhelpdir=	${helpdir}/users
ohelpdir=	${helpdir}/opers

CLEANFILES= users/index users/index.tmp opers/index opers/index.tmp

SYMLINKS=	topic accept cmode admin names links away whowas \
		version kick who invite quit join list nick oper part \
		time credits motd userhost users whois ison lusers \
		user help pass error challenge knock ping pong \
		map trace chantrace extban monitor batch

all:
build:
depend:
lint:

index:
	@$(SHELL) ./make_index.SH ./ $(helpdir)

install: index
	@echo "ircd: setting up help directory structure"
	@mkdir -p -m 755 $(DESTDIR)$(helpdir)
	@mkdir -p -m 755 $(DESTDIR)$(helpdir)/opers
	@mkdir -p -m 755 $(DESTDIR)$(helpdir)/users

	@for help in opers/*; do \
	if [ -f $$help ]; then \
		${INSTALL_DATA} $$help $(DESTDIR)$(ohelpdir); \
	fi \
	done
	@for help in users/*; do \
	if [ -f $$help ]; then \
		$(INSTALL_DATA) $$help $(DESTDIR)$(uhelpdir); \
	fi \
	done
	@for link in $(SYMLINKS); do \
		rm -f $(DESTDIR)$(uhelpdir)/$$link; \
		ln -s $(ohelpdir)/$$link $(DESTDIR)$(uhelpdir); \
	done
