default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
DEFS = -DVERSION_STRING="$(REPO_VER)"

INCLUDE=-I$(MAS_INCLUDE_F)

LIBRARY=-L$(MCELIB_F) -l$(MCELIB) \
	-L$(LOGLIB_F) -l$(LOGLIB) \
	-lpthread -lconfig 

CFLAGS += $(INCLUDE) $(DEFS) -Wall

OBJECTS = ramp.o options.o main.o
HEADERS = ramp.h options.h

CC=gcc

# targets

all: mce_ramp

mce_ramp: $(OBJECTS) $(HEADERS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LIBRARY)

$(OBJECTS): $(HEADERS)

tidy:
	-rm *~ *.o

clean:	tidy
	-rm mce_ramp

# Make the install__mce_ramp rule
INSTALL_TARGET=mce_ramp
include $(MAKERULES)Makefile.install_rule

install: install__mce_ramp

