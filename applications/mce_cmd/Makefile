default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
DEFS += -DVERSION_STRING="$(REPO_VER)"

LIBRARY=$(MCE_LIBS)

CFLAGS += $(DEFS)

# targets

OBJECTS = mce_cmd.o options.o
HEADERS = options.h cmd.h ../../defaults/config.h $(LIBHEADERS)

all: mce_cmd

mce_cmd: mce_cmd.o options.o $(LIBDEP)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

$(OBJECTS): $(HEADERS)

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f mce_cmd

# Make the install__mce_cmd rule
INSTALL_TARGET=mce_cmd
include $(MAKERULES)Makefile.install_rule

install: install__mce_cmd

