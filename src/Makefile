# 
# Copyright (c) 2003-2004 E. Will et al.
# Rights to this code are documented in doc/LICENSE.
# 
# This file contains build instructions.
# 
# $$Id$
# 

include ../Makefile.inc

VERSION=devel

OBJS=cmode.o conf.o db.o event.o function.o help.o irc.o match.o node.o set.o services.o shrike.o socket.o

all: version shrike

clean:
	$(RM) -f *.o shrike;

distclean: clean
	$(RM) -f ../inc/version.h ../inc/sysconf.h ../config.* ../Makefile.inc

version:
	sh version.sh $(VERSION);

install: all
	$(INSTALL) -m 755 -d $(PREFIX)
	$(INSTALL) -m 755 -d $(PREFIX)/bin
	$(INSTALL) -m 750 -d $(PREFIX)/etc
	$(INSTALL) -m 755 -d $(PREFIX)/var
	$(INSTALL) -m 755 -c $(BIN) $(PREFIX)/bin
	if [ ! -r $(PREFIX)/etc/shrike.conf ]; then \
		$(INSTALL) -m 640 -c ../dist/shrike.conf $(PREFIX)/etc ; \
	fi
	if [ ! -r $(PREFIX)/etc/shrike.db ]; then \
		$(INSTALL) -m 640 -c ../dist/shrike.db $(PREFIX)/etc ; \
	fi
	if [ -e $(PREFIX)/doc ] ; then \
		$(RM) -rf $(PREFIX)/doc ; \
	fi
	if [ -e $(PREFIX)/help ] ; then \
		$(RM) -rf $(PREFIX)/help ; \
	fi
	$(CP) -R ../doc $(PREFIX)
	$(CP) -R ../help $(PREFIX)

	@echo "----------------------------------------------------------------"
	@echo ">>> Remember to cd to ${PREFIX} and edit your config file.";
	@echo "----------------------------------------------------------------"

deinstall:
	if [ -d ${PREFIX} ] ; then \
		$(RM) -rf ${PREFIX}; \
	fi

shrike: $(OBJS)
	 $(CC) $(OBJS) ${LFLAGS} ${LIBS} -o $@
	 $(RM) -f shrike.o;

.c.o:
	$(CC) $(CFLAGS) -c $<
