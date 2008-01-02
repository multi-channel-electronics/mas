default: all

BASE := ../
include $(BASE)Makefile.children

LIB     = mcecmd
LIBNAME = lib$(LIB).a

DSP_DIR = ../driver
OBJECTS = cmd.o data.o acq.o data_thread.o config.o files.o frame.o errors.o
HEADERS = data_thread.h frame.h \
	$(MAS_INCLUDE_F)mcecmd.h $(MAS_INCLUDE_F)mcedata.h $(MAS_INCLUDE_F)mceconfig.h

INCLUDE += -I. -I$(MAS_INCLUDE_F) -I$(LOGLIB_F) -I$(DSP_DIR)

CFLAGS += -O2 -Wall $(INCLUDE)

CC = gcc

all: $(LIBNAME)

INSTALL_TARGET=$(LIBNAME)
include $(MAKERULES)Makefile.install_rule_lib

install: install__$(LIBNAME)

$(LIBNAME) : $(OBJECTS) $(HEADERS)
	$(AR) rs $@ $(OBJECTS)

cmd.o : cmd.c $(HEADERS)
	$(CC) $(CFLAGS) cmd.c -c

data.o: data.c $(HEADERS)
	$(CC) $(CFLAGS) data.c -c

config.o: config.c $(HEADERS)
	$(CC) $(CFLAGS) $< -c

files.o: files.c $(HEADERS)
	$(CC) $(CFLAGS) $< -c

cfgtest: cfgtest.c config.o
	$(CC) $(CFLAGS) cfgtest.c config.o -lconfig -o $@

docs:
	doxygen

clean: tidy
	-rm $(LIBNAME)

tidy:
	-rm -f core.* *~ *.c~ *.h~ core.* *.o #*#

