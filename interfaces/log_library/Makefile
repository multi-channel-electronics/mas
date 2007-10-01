
# Target name for logger client library
LOGLIB = libmaslog.a

SOCK = ../misc/socks.o
INCLUDE = -I ../misc

CC = gcc

CFLAGS += $(INCLUDE)

default: $(LOGLIB)

all: $(LOGLIB)

$(LOGLIB): $(SOCK) libmaslog.o libmaslog.h
	$(AR) rs $@ $(SOCK) libmaslog.o

tidy:
	-rm *~ *.o

clean:	tidy
	-rm $(LOGLIB)
