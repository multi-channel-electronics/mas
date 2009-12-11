default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
include $(MAKERULES)Makefile.defaults
DEFS += -DVERSION_STRING="$(REPO_VER)" $(CONFIGFILE_DEFS)

LIBRARY=$(MAS_LIB) $(MASMISC_F)cmdtree.o

CFLAGS += $(DEFS)

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

