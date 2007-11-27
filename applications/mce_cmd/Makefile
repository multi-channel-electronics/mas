default: all

BASE := ../
include $(BASE)Makefile.children

INCLUDE=-I$(MCELIB_F) -I$(MAS_INCLUDE_F) -I$(MASMISC_F)

LIBRARY=-L$(MCELIB_F) -l$(MCELIB) \
	-L$(LOGLIB_F) -l$(LOGLIB) \
	$(MASMISC_F)cmdtree.o \
	-lpthread -lconfig 

CFLAGS += $(INCLUDE) -Wall

CC=gcc

# targets

all: mce_cmd

mce_cmd: mce_cmd.o options.o options.h
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

tidy:
	-rm *~ *.o

clean:	tidy
	-rm mce_cmd

# Make the install__mce_cmd rule
INSTALL_TARGET=mce_cmd
include $(MAKERULES)Makefile.install_rule

install: install__mce_cmd

