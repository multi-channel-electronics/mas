default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
DEFS += -DVERSION_STRING="$(REPO_VER)"

INCLUDE=-I$(MAS_INCLUDE_F)

LIBRARY=-L$(MCELIB_F) -l$(MCELIB) \
	-L$(LOGLIB_F) -l$(LOGLIB) \
	$(MASMISC_F)cmdtree.o \
	-lpthread -lconfig 

CFLAGS += $(INCLUDE) -Wall $(DEFS)

CC=gcc

# targets

HEADERS = options.h cmd.h

all: mce_cmd

mce_cmd: mce_cmd.o options.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

.c.o: 	$(HEADERS)

tidy:
	-rm *~ *.o

clean:	tidy
	-rm mce_cmd

# Make the install__mce_cmd rule
INSTALL_TARGET=mce_cmd
include $(MAKERULES)Makefile.install_rule

install: install__mce_cmd

