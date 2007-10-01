LIBNAME = libmcedsp.a
HDRNAME = mcedsp.h
DESTDIR = /usr/local/share/libmcedsp/
LINKDIR = /usr/lib/
LINCDIR = /usr/include/

CC = gcc

INCLUDE += -I ../../include -I.

CFLAGS += -O2 -Wall -fPIC $(INCLUDE)

default: $(LIBNAME)

all: $(LIBNAME)

install: $(LIBNAME)
	-mkdir $(DESTDIR)
	cp $(LIBNAME) $(HDRNAME) $(DESTDIR)
	-ln -s $(DESTDIR)$(LIBNAME) $(LINKDIR)$(LIBNAME)
	-ln -s $(DESTDIR)$(HDRNAME) $(LINCDIR)$(HDRNAME)

shared : dsp_library.o
	$(LD) -shared -soname libmcedsp.so.1 -o libmcedsp.so.1.0 -lc dsp_library.o

$(LIBNAME): dsp_library.o
	$(AR) rs $@ $<

dsp_library.o: dsp_library.c $(HDRNAME)
	$(CC) $(CFLAGS) dsp_library.c -c

clean:
	rm -f core.* *~ *.c~ *.h~ core.* *.o *.a
