default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
include $(MAKERULES)Makefile.defaults
DEFS += -DVERSION_STRING="$(REPO_VER)" $(CONFIGFILE_DEFS)

INCLUDE=-I$(MAS_INCLUDE_F)

LIBRARY=-L$(MCELIB_F) -l$(MCELIB) \
	-L$(LOGLIB_F) -l$(LOGLIB) \
	$(MASMISC_F)cmdtree.o \
	-lpthread -lconfig -lreadline

CFLAGS += $(INCLUDE) -Wall -std=c99 $(DEFS)

CC=gcc

# targets

OBJECTS = mas_mon.o
HEADERS = 

all: mas_mon

mas_mon: mas_mon.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

$(OBJECTS): $(HEADERS)

tidy:
	-rm *~ *.o

clean:	tidy
	-rm mas_mon

# Make the install__raw_acq rule
INSTALL_TARGET=mas_mon
include $(MAKERULES)Makefile.install_rule

install: install__mas_mon

