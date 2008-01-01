default: all

BASE := ../
include $(BASE)Makefile.children

LIB     = mcecmd
LIBNAME = lib$(LIB).a
LIBHDR  = $(LIB).h

DSP_DIR = ../driver
OBJECTS = cmd.o data.o acq.o data_thread.o config.o files.o frame.o errors.o
HEADERS = $(LIBHDR) mcedata.h mceconfig.h

INCLUDE += -I. -I$(MAS_INCLUDE_F) -I$(LOGLIB_F) -I$(DSP_DIR)

CFLAGS += -O2 -Wall $(INCLUDE)

CC = gcc

all: $(LIBNAME)

INSTALL_TARGET=$(LIBNAME)
include $(MAKERULES)Makefile.install_rule_lib

INSTALL_TARGET=mcecmd.h
include $(MAKERULES)Makefile.install_rule_inc

INSTALL_TARGET=mcedata.h
include $(MAKERULES)Makefile.install_rule_inc

INSTALL_TARGET=mceconfig.h
include $(MAKERULES)Makefile.install_rule_inc

install: install__$(LIBNAME) install__mcecmd.h install__mcedata.h install__mceconfig.h

$(LIBNAME) : $(OBJECTS) $(HEADERS)
	$(AR) rs $@ $(OBJECTS)

cmd.o : cmd.c $(HEADERS)
	$(CC) $(CFLAGS) cmd.c -c

data.o: data.c $(HEADERS)
	$(CC) $(CFLAGS) data.c -c

config.o: config.c mceconfig.h
	$(CC) $(CFLAGS) $< -c

files.o: files.c mceconfig.h mcedata.h
	$(CC) $(CFLAGS) $< -c

cfgtest: cfgtest.c config.o
	$(CC) $(CFLAGS) cfgtest.c config.o -lconfig -o $@

docs:
	doxygen

clean: tidy
	rm $(LIBNAME)

tidy:
	rm -f core.* *~ *.c~ *.h~ core.* *.o #*#

