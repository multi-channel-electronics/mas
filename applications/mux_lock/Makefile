default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
DEFS += -DVERSION_STRING="$(REPO_VER)"

LIBRARY=$(MCE_LIBS)

CFLAGS += $(DEFS)

# targets

TARGETS = sq1servo sq2servo sq1servo_all sq1servo_sa rs_servo

OBJECTS = $(foreach d,$(TARGETS),$(d).o ) \
	servo.o options.o

HEADERS = servo.h servo_err.h options.h ../../defaults/config.h

.c.o :
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJECTS): $(HEADERS)

all: $(TARGETS)

sq1servo: sq1servo.o servo.o options.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

sq1servo_all: sq1servo_all.o servo.o options.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

sq1servo_sa: sq1servo_sa.o servo.o options.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

rs_servo: rs_servo.o servo.o options.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

sq2servo: sq2servo.o servo.o options.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBRARY)

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f $(TARGETS)

# Make the install__*servo rules
INSTALL_TARGET=sq1servo
include $(MAKERULES)Makefile.install_rule
INSTALL_TARGET=sq1servo_all
include $(MAKERULES)Makefile.install_rule
INSTALL_TARGET=sq1servo_sa
include $(MAKERULES)Makefile.install_rule
INSTALL_TARGET=rs_servo
include $(MAKERULES)Makefile.install_rule
INSTALL_TARGET=sq2servo
include $(MAKERULES)Makefile.install_rule

install: install__sq1servo install__sq1servo_all install__sq2servo

