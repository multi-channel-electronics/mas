default: check

BASE = ../
include $(BASE)Makefile.children

HEADERS = mcecmd.h mceconfig.h mcedata.h \
	libmaslog.h mcedsp.h mce_library.h

HEADERS_MCE = $(shell ls mce |sed 's/^/mce\//g' )

check: $(HEADERS) $(HEADERS_MCE)
	@echo "Headers are present, do 'make install' to install them."

install: $(HEADERS) $(HEADERS_MCE) mas_base_install
	-for h in $(HEADERS); do \
		[ -h $(MAS_SYS_INC)$$h ] && rm $(MAS_SYS_INC)$$h ; \
	done
	for h in $(HEADERS); do \
		install -m ug+rw,o+r --owner=$(MAS_USER) --group=$(MAS_GROUP) \
			$$h $(MAS_INSTALL_INC); \
		ln -s $(MAS_INSTALL_INC)$$h $(MAS_SYS_INC)$$h ; \
	done
	install -d $(MAS_INSTALL_INC)/mce
	-[ -h $(MAS_SYS_INC)mce ] && rm -rf $(MAS_SYS_INC)mce
	ln -s $(MAS_INSTALL_INC)mce $(MAS_SYS_INC)mce
	for h in $(HEADERS_MCE); do \
		install -m ug+rw,o+r --owner=$(MAS_USER) --group=$(MAS_GROUP) \
			$$h $(MAS_INSTALL_INC)mce; \
	done

clean:
	rm -f core.* *~ *.c~ *.h~ core.* *.o *.a
