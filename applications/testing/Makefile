default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version

# targets
TARGETS = mce_state

OBJECTS = mce_state.o
HEADERS = 

all: $(TARGETS)

mce_state: mce_state.o
	$(CC) $(CFLAGS) $^ -o $@ $(MAS_LIB)

$(OBJECTS): $(HEADERS)

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f $(TARGETS)

install:
