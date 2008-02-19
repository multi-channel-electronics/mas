default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
DEFS += -DVERSION_STRING="$(REPO_VER)"

INCLUDE=-I$(MAS_INCLUDE_F)

LIBRARY=-L$(MCELIB_F) -l$(MCELIB) \
	-L$(LOGLIB_F) -l$(LOGLIB) \
	-lpthread -lconfig 

CFLAGS += $(INCLUDE) -Wall $(DEFS)

CC=gcc

# targets

TARGETS = sq1servo   sq2servo
OBJECTS = sq1servo.o sq2servo.o servo.o
HEADERS = servo.h servo_err.h options.h

all: $(TARGETS)

$(OBJECTS): $(HEADERS)

sq1servo: sq1servo.o servo.o options.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

sq2servo: sq2servo.o servo.o options.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

.c.o: 	$(HEADERS)

tidy:
	-rm *~ *.o

clean:	tidy
	-rm $(TARGETS)

# Make the install__*servo rules
INSTALL_TARGET=sq1servo
include $(MAKERULES)Makefile.install_rule
INSTALL_TARGET=sq2servo
include $(MAKERULES)Makefile.install_rule

install: install__sq1servo install__sq2servo

