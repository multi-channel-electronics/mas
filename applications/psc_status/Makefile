default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
DEFS += -DVERSION_STRING="$(REPO_VER)"

INCLUDE=-I$(MAS_INCLUDE_F)

LIBRARY=-L$(MCELIB_F) -l$(MCELIB) \
	-L$(LOGLIB_F) -l$(LOGLIB) \
	-lpthread -lconfig 

CFLAGS += $(INCLUDE) -Wall $(DEFS)

CC=gcc

# targets

TARGETS = psc_status 
OBJECTS = psc_status.o options.o
HEADERS = psc_status.h options.h

all: $(TARGETS)

$(OBJECTS): $(HEADERS)

psc_status: psc_status.o options.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)


.c.o: 	$(HEADERS)

tidy:
	-rm *~ *.o

clean:	tidy
	-rm $(TARGETS)

# Make the install__psc_status rules
INSTALL_TARGET=psc_status
include $(MAKERULES)Makefile.install_rule

install: install__psc_status 

