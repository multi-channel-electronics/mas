default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
include $(MAKERULES)Makefile.defaults
DEFS = -DVERSION_STRING="$(REPO_VER)" $(CONFIGFILE_DEFS)

LIBRARY=$(MAS_LIB)

CFLAGS += $(DEFS)

OBJECTS = mce_status.o options.o das.o mas.o cfg_dump.o
HEADERS = mce_status.h das.h mas.h cfg_dump.h

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

