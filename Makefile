# Makefile for mod_rpaf.c (gmake)
# $Id: Makefile 16 2007-12-13 03:40:22Z thomas $
APXS=$(shell which apxs) 
APXS2=$(shell which apxs2)

default:
	@echo mod_rpaf:
	@echo nevest version available at http://stderr.net/apache/rpaf/
	@echo
	@echo following options available:
	@echo \"make rpaf\" to compile the 1.3 version
	@echo \"make test\" to test 1.3 version
	@echo \"make install\" to install the 1.3 version
	@echo \"make rpaf-2.0\" to compile the 2.0 version
	@echo \"make test-2.0\" to test 2.0 version
	@echo \"make install-2.0\" to install the 2.0 version
	@echo
	@echo change path to apxs if this is not it: \"$(APXS)\"


rpaf: mod_rpaf.so
	@echo make done
	@echo type \"make test\" to test mod_rpaf
	@echo type \"make install\" to install mod_rpaf

test: rpaf
	@./gen_tests.sh
	cd t && $(MAKE) test
	@echo all done

rpaf-2.0: mod_rpaf-2.0.o
	@echo make done, type \"make install-2.0\" to install mod_rpaf-2.0

test-2.0: rpaf-2.0
	@./gen_tests.sh
	cd t && make test-2.0

mod_rpaf.so: mod_rpaf.c
	$(APXS) -c -o $@ mod_rpaf.c

mod_rpaf.c:

mod_rpaf-2.0.o: mod_rpaf-2.0.c
	$(APXS2) -c -n $@ mod_rpaf-2.0.c

mod_rpaf-2.0.c:

install: mod_rpaf.so
	$(APXS) -i -n mod_rpaf mod_rpaf.so

install-2.0: mod_rpaf-2.0.o
	$(APXS2) -i -n mod_rpaf-2.0.so mod_rpaf-2.0.la

clean:
	rm -rf *~ *.o *.so *.lo *.la *.slo *.loT .libs/ 
	cd t && make clean
