default: all

BASE = ../
include $(BASE)Makefile.children

OBJECTS = socks.o cmdtree.o masconfig.o

HEADERS = $(MAS_INCLUDE_F)mce/socks.h $(MAS_INCLUDE_F)mce/cmdtree.h \
	$(MAS_INCLUDE_F)masconfig.h

all: $(OBJECTS)

socks.o: socks.c $(MAS_INCLUDE_F)mce/socks.h
	$(CC) $(CFLAGS) $< -c -o $@

cmdtree.o: cmdtree.c $(MAS_INCLUDE_F)mce/cmdtree.h
	$(CC) $(CFLAGS) $< -c -o $@

masconfig.o: masconfig.c $(MAS_INCLUDE_F)masconfig.h
	$(CC) $(CFLAGS) $< -c -o $@

test.o: test.c $(HEADERS)

test: test.o cmdtree.o $(HEADERS)
	$(CC) $(CFLAGS) test.o cmdtree.o -o test

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f $(OBJECT)
