default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version

# targets
TARGETS = mce_state

OBJECTS = mce_state.o peek.o
HEADERS = $(LIBHEADERS)

all: $(TARGETS)

%: %.o $(LIBDEP)
	$(CC) $(CFLAGS) $< -o $@ $(MCE_LIBS)

$(OBJECTS): $(HEADERS)

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f $(TARGETS)

install:
