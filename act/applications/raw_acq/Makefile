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
	-lpthread -lconfig -lreadline

CFLAGS += $(INCLUDE) -Wall -std=c99 $(DEFS)

CC=gcc

# targets

OBJECTS = raw_acq.o
HEADERS = 

all: raw_acq

raw_acq: raw_acq.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

$(OBJECTS): $(HEADERS)

tidy:
	-rm *~ *.o

clean:	tidy
	-rm raw_acq

# Make the install__raw_acq rule
INSTALL_TARGET=raw_acq
include $(MAKERULES)Makefile.install_rule

install: install__raw_acq

