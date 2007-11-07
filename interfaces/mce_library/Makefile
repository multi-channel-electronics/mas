LIB     = mcecmd
LIBNAME = lib$(LIB).a
LIBHDR  = $(LIB).h
DESTDIR = /usr/local/share/libmcedsp
LINKDIR = /usr/lib/
LINCDIR = /usr/include/

CC = gcc

DSP_DIR = ../driver
OBJECTS = mce_library.o mcedata.o mce_xml.o mceconfig.o files.o
HEADERS = $(LIBHDR) mcedata.h mce_xml.h mceconfig.h

INCLUDE += -I. -I../../include -I../log_library
LIBRARY += -L../log_libary

CFLAGS += -O2 -Wall -I$(DSP_DIR) $(LIBRARY) $(INCLUDE) -DUSE_CFG

default: $(LIBNAME)

all: $(LIBNAME)

install: $(LIBNAME) $(LIBHDR)
	-mkdir $(DESTDIR)
	cp $(LIBNAME) $(LIBHDR) mcedata.h $(DESTDIR)
	-ln -s $(DESTDIR)$(LIBNAME) $(LINKDIR)$(LIBNAME)
	-ln -s $(DESTDIR)$(LIBHDR) $(LINCDIR)$(LIBHDR)
	-ln -s $(DESTDIR)mcedata.h $(LINCDIR)mcedata.h

$(LIBNAME) : $(OBJECTS) $(HEADERS)
	$(AR) rs $@ $(OBJECTS)

mce_library.o : mce_library.c # $(HEADERS)
	$(CC) $(CFLAGS) mce_library.c -c

mcedata.o: mcedata.c $(HEADERS)
	$(CC) $(CFLAGS) mcedata.c -c

mceconfig.o: mceconfig.c mceconfig.h
	$(CC) $(CFLAGS) $< -c

files.o: files.c mceconfig.h mcedata.h
	$(CC) $(CFLAGS) $< -c

mce_xml.o: mce_xml.c mce_xml.h
	$(CC) $(CFLAGS) mce_xml.c -c

cfgtest: cfgtest.c mceconfig.o
	$(CC) $(CFLAGS) cfgtest.c mceconfig.o -lconfig -o $@

xmltest: $(LIBNAME) xmltest.c
	$(CC) $(CFLAGS) xmltest.c -o $@ -lexpat -lmcecmd -L.

docs:
	doxygen

clean:
	rm -f core.* *~ *.c~ *.h~ core.* *.o *.a

