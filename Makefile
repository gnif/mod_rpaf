# Makefile for mod_rpaf.c (gmake)
# $Id: Makefile 16 2007-12-13 03:40:22Z thomas $
APXS=./apxs.sh

rpaf: mod_rpaf.so
	@echo make done
	@echo type \"make install\" to install mod_rpaf

mod_rpaf.so: mod_rpaf.c
	$(APXS) -c -n $@ mod_rpaf.c

mod_rpaf.c:

install: mod_rpaf.so
	$(APXS) -i -S LIBEXECDIR=$(DESTDIR)$$($(APXS) -q LIBEXECDIR)/ -n mod_rpaf.so mod_rpaf.la

clean:
	rm -rf *~ *.o *.so *.lo *.la *.slo *.loT .libs/
