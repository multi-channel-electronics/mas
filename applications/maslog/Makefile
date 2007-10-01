
# Target name for logger application

LOGAPP = maslog_server
CLIENT = maslog_now

# logger uses multithreading, and both use libconfig

CC=gcc

LIB = -L/usr/local/lib -lconfig

INTERFACES=../../interfaces

SOCK = $(INTERFACES)/misc/socks.o
LOGLIB = -L$(INTERFACES)/log_library -lmaslog

SOURCE = logger.c main.c init.c
HEADER = logger.h init.h

INCLUDE += -I$(INTERFACES)/misc -I$(INTERFACES)/log_library

CFLAGS += $(INCLUDE)

default: all

all: $(LOGAPP) $(CLIENT)

$(LOGAPP): $(SOURCE) $(HEADER)
	$(CC) $(CFLAGS) $(SOURCE) -o$@ $(SOCK) $(LIB)

$(CLIENT): client.c
	$(CC) $(CFLAGS) $< -o$@ $(LIB) $(LOGLIB)

tidy:
	-rm *~ *.o

clean:	tidy
	-rm $(LOGAPP)
	-rm $(CLIENT)
