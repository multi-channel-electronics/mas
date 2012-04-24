default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
DEFS += -DVERSION_STRING="$(REPO_VER)"

LIBRARY=$(MCE_LIBS)
CFLAGS += $(DEFS)

# targets

OBJECTS = raw_acq.o
HEADERS = 

all: raw_acq

raw_acq: raw_acq.o $(LIBDEP) $(LIBHEADERS)
	$(CC) $(CFLAGS) raw_acq.o -o raw_acq $(LIBRARY)

$(OBJECTS): $(HEADERS)

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f raw_acq

# Make the install__raw_acq rule
INSTALL_TARGET=raw_acq
include $(MAKERULES)Makefile.install_rule

install: install__raw_acq

