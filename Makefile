# Makefile for mod_rpaf.c (gmake)
# $Id: Makefile 16 2007-12-13 03:40:22Z thomas $
APXS=./apxs.sh

mod_rpaf.la: mod_rpaf.c
	$(APXS) -c -n $@ mod_rpaf.c
	@echo make done
	@echo type \"make install\" to install mod_rpaf

install: mod_rpaf.la
	$(APXS) -i -S LIBEXECDIR=$(DESTDIR)$$($(APXS) -q LIBEXECDIR)/ -n mod_rpaf.so mod_rpaf.la

clean:
	rm -rf *~ *.o *.so *.lo *.la *.slo *.loT .libs/
