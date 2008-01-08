default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
DEFS = -DVERSION_STRING="$(REPO_VER)"

INCLUDE=-I$(MAS_INCLUDE_F)

LIBRARY=-L$(MCELIB_F) -l$(MCELIB) \
	-L$(LOGLIB_F) -l$(LOGLIB) \
	-lpthread -lconfig 

CFLAGS += $(INCLUDE) $(DEFS) -Wall

OBJECTS = mce_status.o options.o das.o cfg_dump.o
HEADERS = mce_status.h das.h cfg_dump.h

CC=gcc

# targets

all: mce_status

mce_status: $(OBJECTS) $(HEADERS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LIBRARY)

options.o: $(HEADERS)

mce_status.o: $(HEADERS)

tidy:
	-rm *~ *.o

clean:	tidy
	-rm mce_status

# Make the install__mce_status rule
INSTALL_TARGET=mce_status
include $(MAKERULES)Makefile.install_rule

install: install__mce_status

