default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
DEFS += -DVERSION_STRING="$(REPO_VER)"

LIBRARY=$(MCE_LIBS)

CFLAGS += $(DEFS)

# targets

TARGETS = sq1servo sq2servo sq1servo_all sq1servo_sa

OBJECTS = $(foreach d,$(TARGETS),$(d).o ) \
	servo.o options.o

HEADERS = servo.h servo_err.h options.h ../../defaults/config.h

$(OBJECTS): $(HEADERS)

all: $(TARGETS)

sq1servo: sq1servo.o servo.o options.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

sq1servo_all: sq1servo_all.o servo.o options.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

sq1servo_sa: sq1servo_sa.o servo.o options.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

sq1servo_rs: sq1servo_rs.o servo.o options.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

sq2servo: sq2servo.o servo.o options.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

.c.o: 	$(HEADERS)

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f $(TARGETS)

# Make the install__*servo rules
INSTALL_TARGET=sq1servo
include $(MAKERULES)Makefile.install_rule
INSTALL_TARGET=sq1servo_all
include $(MAKERULES)Makefile.install_rule
INSTALL_TARGET=sq2servo
include $(MAKERULES)Makefile.install_rule

install: install__sq1servo install__sq1servo_all install__sq2servo

