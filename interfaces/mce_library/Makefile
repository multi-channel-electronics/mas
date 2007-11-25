LIB     = mcecmd
LIBNAME = lib$(LIB).a
LIBHDR  = $(LIB).h
DESTDIR = /usr/local/share/libmcedsp/
LINKDIR = /usr/lib/
LINCDIR = /usr/include/

CC = gcc

DSP_DIR = ../driver
OBJECTS = cmd.o data.o data_thread.o config.o files.o frame.o
HEADERS = $(LIBHDR) mcedata.h mceconfig.h

INCLUDE += -I. -I../../include -I../log_library
LIBRARY += -L../log_libary

CFLAGS += -O2 -Wall -I$(DSP_DIR) $(LIBRARY) $(INCLUDE) -DUSE_CFG

default: $(LIBNAME)

all: $(LIBNAME)

install: $(LIBNAME) $(LIBHDR)
	-mkdir $(DESTDIR)
	cp $(LIBNAME) $(HEADERS) $(DESTDIR)
	-ln -s $(DESTDIR)$(LIBNAME) $(LINKDIR)$(LIBNAME)
	-ln -s $(DESTDIR)$(LIBHDR) $(LINCDIR)$(LIBHDR)
	-ln -s $(DESTDIR)mcedata.h $(LINCDIR)mcedata.h
	-ln -s $(DESTDIR)mceconfig.h $(LINCDIR)mceconfig.h

$(LIBNAME) : $(OBJECTS) $(HEADERS)
	$(AR) rs $@ $(OBJECTS)

cmd.o : cmd.c # $(HEADERS)
	$(CC) $(CFLAGS) cmd.c -c

data.o: data.c $(HEADERS)
	$(CC) $(CFLAGS) data.c -c

config.o: config.c mceconfig.h
	$(CC) $(CFLAGS) $< -c

files.o: files.c mceconfig.h mcedata.h
	$(CC) $(CFLAGS) $< -c

cfgtest: cfgtest.c config.o
	$(CC) $(CFLAGS) cfgtest.c config.o -lconfig -o $@

docs:
	doxygen

clean:
	rm -f core.* *~ *.c~ *.h~ core.* *.o *.a

