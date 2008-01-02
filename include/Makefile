default: install

BASE = ../
include $(BASE)Makefile.children

HEADERS = mcecmd.h mceconfig.h mcedata.h \
	libmaslog.h mcedsp.h \
	mce.h mce_errors.h \
	dsp.h dsp_errors.h

install: $(HEADERS) mas_base_install
	@for h in $(HEADERS); do \
		install -m ug+rw,o+r --owner=$(MAS_USER) --group=$(MAS_GROUP) \
			$$h $(MAS_INSTALL_INC); \
		if [ -f $(MAS_SYS_INC)$$h ]; then rm $(MAS_SYS_INC)$$h ; fi ; \
		ln -s $(MAS_INSTALL_INC)$$h $(MAS_SYS_INC)$$h ; \
	done

clean:
	rm -f core.* *~ *.c~ *.h~ core.* *.o *.a

