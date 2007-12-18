
# Target names for logger application and client library

DATAAPP = masdata_server
DATALIB = libmasdata.a
CLIENT = masdata

# logger uses multithreading, and both use libconfig

CC=gcc

INTERFACES=../../interfaces

INCLUDE += -I../../include \
	 -I$(INTERFACES)/log_library \
	 -I$(INTERFACES)/mce_library \
	 -I$(INTERFACES)/misc
 
LIB += -lpthread -I../../include \
	 -I$(INTERFACES)/log_library -L$(INTERFACES)/log_library \
	 -lmaslog \
	 -I$(INTERFACES)/mce_library -L$(INTERFACES)/mce_library -lmcecmd \
	 -I$(INTERFACES)/misc

LIBCONFIG = -L/usr/local/lib -lconfig
SOCK=$(INTERFACES)/misc/socks.o

CFLAGS += -Wall -Wstrict-prototypes $(INCLUDE)

SOURCE = data.c server.c main.c init.c files.c frames.c mcecomms.c
HEADERS = main.h data.h init.h frames.h files.h masdata.h mcecomms.h


default: all

all: $(DATAAPP) $(DATALIB) $(CLIENT)

$(DATAAPP): $(SOURCE) $(HEADERS) $(SOCK)
	$(CC) $(CFLAGS) $(SOURCE) -o$@ $(SOCK) $(LIB) $(LIBCONFIG)

$(DATALIB): $(SOCK) libmasdata.o libmasdata.h
	$(AR) rs $@ $(SOCK) libmasdata.o

$(CLIENT): control.c $(DATALIB)
	$(CC) $(CFLAGS) -o$@ $<  $(LIB) -L. -lmasdata $(LIBCONFIG)

libmasdata.o: libmasdata.c libmasdata.h
	$(CC) $(CFLAGS) -c $<

tidy:
	-rm *~ *.o

clean:	tidy
	-rm $(DATAAPP) $(DATALIB) $(CLIENT)
