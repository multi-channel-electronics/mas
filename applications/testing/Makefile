default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version

# targets
TARGETS = mce_state eth_server

OBJECTS = mce_state.o
HEADERS = 

all: $(TARGETS)

mce_state: mce_state.o
	$(CC) $(CFLAGS) $^ -o $@ $(MCE_LIBS)

eth_server: eth_server.c
	$(CC) $(CFLAGS) -I../../interfaces/mce_library/ $^ -o $@ $(MCE_LIBS)

$(OBJECTS): $(HEADERS)

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f $(TARGETS)

install:
