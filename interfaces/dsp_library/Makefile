default: all

BASE := ../
include $(BASE)Makefile.children

LIBNAME = lib$(DSPLIB).a

HEADERS = $(MAS_INCLUDE_F)mcedsp.h

all: $(LIBNAME)

$(LIBNAME): dsp_library.o errors.o
	$(AR) rs $@ $^

dsp_library.o: dsp_library.c $(HEADERS)
	$(CC) $(CFLAGS) dsp_library.c -c


INSTALL_TARGET=$(LIBNAME)
include $(MAKERULES)Makefile.install_rule_lib

install: install__$(LIBNAME)



clean: tidy
	-rm $(LIBNAME)

tidy:
	-rm -f core.* *~ *.c~ *.h~ core.* *.o *.a
