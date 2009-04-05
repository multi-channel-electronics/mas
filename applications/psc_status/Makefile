default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
include $(MAKERULES)Makefile.defaults
DEFS += -DVERSION_STRING="$(REPO_VER)" $(CONFIGFILE_DEFS)

LIBRARY=$(MAS_LIB)

CFLAGS += $(DEFS)

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

