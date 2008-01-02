default: install

BASE = ../
include $(BASE)Makefile.children

HEADERS = mce.h mce_errors.h dsp.h dsp_errors.h

install: $(HEADERS)
	@for h in $(HEADERS); do \
		cp $$h $(MAS_INSTALL_INC); \
	done

clean:
	rm -f core.* *~ *.c~ *.h~ core.* *.o *.a

