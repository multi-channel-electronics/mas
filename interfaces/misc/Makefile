default: all

BASE = ../
include $(BASE)Makefile.children

OBJECTS = cmdtree.o

HEADERS = $(MAS_INCLUDE_F)mce/cmdtree.h

all: $(OBJECTS)

cmdtree.o: cmdtree.c $(MAS_INCLUDE_F)mce/cmdtree.h
	$(CC) $(CFLAGS) $< -c -o $@

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f $(OBJECT)
