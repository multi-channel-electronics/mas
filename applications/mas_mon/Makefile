default: all

BASE := ..
include $(BASE)/Makefile.children

VER_TARGET := .
include $(MAKERULES)/Makefile.version
DEFS += -DVERSION_STRING="$(REPO_VER)"

LIBRARY=$(MCE_LIBS)

CFLAGS += $(DEFS)

# targets

OBJECTS = mas_mon.o
HEADERS = $(LIBHEADERS)

all: mas_mon

mas_mon: mas_mon.o $(LIBDEP)
	$(CC) $(CFLAGS) mas_mon.o -o $@ $(LIBRARY)

$(OBJECTS): $(HEADERS)

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f mas_mon

# Make the install__raw_acq rule
INSTALL_TARGET=mas_mon
include $(MAKERULES)/Makefile.install_rule

install: install__mas_mon

