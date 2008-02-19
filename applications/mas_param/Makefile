default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
DEFS = -DVERSION_STRING="$(REPO_VER)"

INCLUDE=-I$(MAS_INCLUDE_F)

LIBRARY=-L$(LOGLIB_F) -l$(LOGLIB) \
	-lconfig $(MASMISC_F)masconfig.o

CFLAGS += $(INCLUDE) $(DEFS) -Wall

OBJECTS = main.o options.o get.o crawl.o bash.o csh.o idl.o
HEADERS = mas_param.h get.h crawl.h bash.h csh.h idl.h

CC=gcc

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

