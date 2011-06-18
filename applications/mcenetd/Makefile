default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
DEFS += -DVERSION_STRING="$(REPO_VER)"

LIBRARY=$(MCE_LIBS) $(DSP_LIBS) $(MASMISC_F)cmdtree.o

CFLAGS += $(DEFS)

# targets

OBJECTS = mcenetd.o
HEADERS = ../../defaults/config.h

all: mcenetd

mcenetd: mcenetd.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

$(OBJECTS): $(HEADERS)

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f mcenetd

# Make the install__mce_cmd rule
INSTALL_TARGET=mcenetd
include $(MAKERULES)Makefile.install_rule

install: install__mcenetd

