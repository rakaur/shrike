#
# Copyright (c) 2003-2004 E. Will et al.
# Rights to this code are documented in doc/LICENSE.
#
# This file contains build instructions.
#
# $$Id$
#

include Makefile.inc

all:
	-@if [ ! -r config.cache ]; then \
		echo "Hmm... looks like you haven't configured."; \
		echo "I'll do that now, then."; \
		sh configure; \
	fi
	@echo ">>> Building binaries"
	cd src; make; cd ..;

clean:
	@echo ">>> Cleaning binaries"
	cd src; make clean; cd ..;

distclean: 
	@echo ">>> Cleaning binaries and configurations"
	cd src; make distclean; cd ..;

install: all
	 @echo ">>> Installing into \`$(PREFIX)\'"
	 @cd src; make install; cd ..;

deinstall:
	@echo ">>> Deinstalling \`$(PREFIX)\'"
	@cd src; make deinstall; cd ..;

uninstall: deinstall
