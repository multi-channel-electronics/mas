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

OBJECTS = mce_cmd.o options.o
HEADERS = options.h cmd.h

all: mce_cmd

mce_cmd: mce_cmd.o options.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

$(OBJECTS): $(HEADERS)

tidy:
	-rm *~ *.o

clean:	tidy
	-rm mce_cmd

# Make the install__mce_cmd rule
INSTALL_TARGET=mce_cmd
include $(MAKERULES)Makefile.install_rule

install: install__mce_cmd

