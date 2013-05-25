default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
DEFS += -DVERSION_STRING="$(REPO_VER)"

LIBRARY=$(MCE_LIBS)

CFLAGS += $(DEFS)

# targets

TARGETS = psc_status 
OBJECTS = psc_status.o options.o
HEADERS = psc_status.h options.h ../../defaults/config.h $(LIBHEADERS)

all: $(TARGETS)

$(OBJECTS): $(HEADERS)

psc_status: psc_status.o options.o $(LIBDEP)
	$(CC) $(CFLAGS) psc_status.o options.o -o $@ $(LIBRARY)


.c.o: 	$(HEADERS)

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f $(TARGETS)

# Make the install__psc_status rules
INSTALL_TARGET=psc_status
include $(MAKERULES)Makefile.install_rule

install: install__psc_status 

