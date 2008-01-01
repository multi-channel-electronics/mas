default: all

BASE := ../
include $(BASE)Makefile.children

LIB     = mcedsp
LIBNAME = lib$(LIB).a
LIBHDR  = $(LIB).h

INCLUDE += -I$(MAS_INCLUDE_F) -I$(LOGLIB_F) -I.

CFLAGS += -O2 -Wall -fPIC $(INCLUDE)

CC = gcc

all: $(LIBNAME)

$(LIBNAME): dsp_library.o errors.o
	$(AR) rs $@ $^

dsp_library.o: dsp_library.c $(HDRNAME)
	$(CC) $(CFLAGS) dsp_library.c -c


INSTALL_TARGET=$(LIBNAME)
include $(MAKERULES)Makefile.install_rule_lib

INSTALL_TARGET=$(LIBHDR)
include $(MAKERULES)Makefile.install_rule_inc

install: install__$(LIBNAME) install__$(LIBHDR)



clean: tidy
	rm $(LIBNAME)

tidy:
	-rm -f core.* *~ *.c~ *.h~ core.* *.o *.a
