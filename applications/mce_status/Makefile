default: all

BASE := ..
include $(BASE)/Makefile.children

VER_TARGET := .
include $(MAKERULES)/Makefile.version
DEFS = -DVERSION_STRING="$(REPO_VER)"

LIBRARY=$(MCE_LIBS)

CFLAGS += $(DEFS)

OBJECTS = mce_status.o options.o das.o mas.o cfg_dump.o dirfile.o
HEADERS = mce_status.h das.h mas.h cfg_dump.h ../../defaults/config.h \
					$(LIBHEADERS)

# targets

all: mce_status

mce_status: $(OBJECTS) $(HEADERS) $(LIBDEP)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LIBRARY)

$(OBJECTS): $(HEADERS)

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f mce_status

# Make the install__mce_status rule
INSTALL_TARGET=mce_status
include $(MAKERULES)/Makefile.install_rule

install: install__mce_status
