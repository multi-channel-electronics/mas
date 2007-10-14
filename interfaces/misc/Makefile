TARGET ?= all

OBJECTS = socks.o cmdtree.o

CC = gcc

default: $(TARGET)

all: $(OBJECTS)

socks.o: socks.c socks.h
	$(CC) $(CFLAGS) $< -c -o $@

cmdtree.o: cmdtree.c cmdtree.h
	$(CC) $(CFLAGS) $< -c -o $@

tidy:
	-rm *~ *.o

clean:	tidy
	-rm $(OBJECT)
