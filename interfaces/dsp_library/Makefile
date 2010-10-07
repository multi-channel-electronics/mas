default: all

BASE := ../
include $(BASE)Makefile.children

LIBNAME = lib$(DSPLIB)

HEADERS = $(MAS_INCLUDE_F)mcedsp.h

all: $(LIBNAME)$(LIB_SUFFIX)

$(LIBNAME).so : dsp_library.o errors.o
	$(CC) -shared -o $@ $^ -lc $(MCE_LIBS) -lpthread

$(LIBNAME).a: dsp_library.o errors.o
	$(AR) rs $@ $^

dsp_library.o: dsp_library.c $(HEADERS)
	$(CC) $(CFLAGS) dsp_library.c -c


INSTALL_TARGET=$(LIBNAME)
include $(MAKERULES)Makefile.install_rule_lib

install: install__$(LIBNAME)



clean: tidy
	rm -f $(LIBNAME).a $(LIBNAME).so

tidy:
	rm -f core.* *~ *.c~ *.h~ core.* *.o *.a
