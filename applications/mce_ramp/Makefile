default: all

BASE := ..
include $(BASE)/Makefile.children

VER_TARGET := .
include $(MAKERULES)/Makefile.version
DEFS = -DVERSION_STRING="$(REPO_VER)"

LIBRARY=

CFLAGS += $(DEFS)

OBJECTS = ramp.o options.o main.o
HEADERS = ramp.h options.h $(LIBHEADERS)

CC=gcc

# targets

all: mce_ramp

mce_ramp: $(OBJECTS) $(HEADERS) $(LIBDEP)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LIBRARY)

$(OBJECTS): $(HEADERS)

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f mce_ramp

# Make the install__mce_ramp rule
INSTALL_TARGET=mce_ramp
include $(MAKERULES)/Makefile.install_rule

install: install__mce_ramp

