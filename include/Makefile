default: all

BASE := ..
include $(BASE)/Makefile.children

HEADERS = mcecmd.h mceconfig.h mcedata.h \
	libmaslog.h mcedsp.h mce_library.h

HEADERS_MCE = $(shell ls mce |sed 's/^/mce\//g' )

all: mce_library.h check

mce_library.h: mce_library.h.in 

check: $(HEADERS) $(HEADERS_MCE)
	@echo "Headers are present, do 'make install' to install them."

install: $(HEADERS) $(HEADERS_MCE) mas_base_install
	for h in $(HEADERS); do \
		install -m ug+rw,o+r --owner=$(MAS_USER) --group=$(MAS_GROUP) \
			$$h $(DESTDIR)/$(MAS_INSTALL_INC); \
	done
	install -d $(DESTDIR)/$(MAS_INSTALL_INC)/mce
	for h in $(HEADERS_MCE); do \
		install -m ug+rw,o+r --owner=$(MAS_USER) --group=$(MAS_GROUP) \
			$$h $(DESTDIR)/$(MAS_INSTALL_INC)mce; \
	done
	if [ ! -z "${MAS_SYS_INC}" ]; then \
		for h in $(HEADERS); do \
			[ -h $(DESTDIR)/$(MAS_SYS_INC)$$h ] && rm -f $(DESTDIR)/$(MAS_SYS_INC)$$h ; \
			ln -s $(MAS_INSTALL_INC)$$h $(DESTDIR)/$(MAS_SYS_INC)$$h ; \
		done; \
		[ -h $(DESTDIR)/$(MAS_SYS_INC)mce ] && rm -rf $(DESTDIR)/$(MAS_SYS_INC)mce; \
		ln -s $(MAS_INSTALL_INC)mce $(DESTDIR)/$(MAS_SYS_INC)mce; \
	fi

clean:
	rm -f core.* *~ *.c~ *.h~ core.* *.o *.a
