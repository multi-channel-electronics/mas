default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
include $(MAKERULES)Makefile.defaults
DEFS = -DVERSION_STRING="$(REPO_VER)" $(CONFIGFILE_DEFS)

LIBRARY=$(MAS_LIB) $(MASMISC_F)masconfig.o

CFLAGS += $(DEFS)

OBJECTS = main.o options.o get.o crawl.o bash.o csh.o idl.o
HEADERS = mas_param.h get.h crawl.h bash.h csh.h idl.h

# targets

all: mas_param

mas_param: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LIBRARY)

$(OBJECTS): $(HEADERS)

tidy:
	-rm *~ *.o

clean:	tidy
	-rm mas_param

# Make the install__mas_param rule
INSTALL_TARGET=mas_param
include $(MAKERULES)Makefile.install_rule

install: install__mas_param

