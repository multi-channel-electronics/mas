TARGET ?= all

OBJECTS = socks.o cmdtree.o

CC = gcc

CFLAGS += -Wall

default: $(TARGET)

all: $(OBJECTS)

socks.o: socks.c socks.h
	$(CC) $(CFLAGS) $< -c -o $@

cmdtree.o: cmdtree.c cmdtree.h
	$(CC) $(CFLAGS) $< -c -o $@

test.o: test.c cmdtree.o cmdtree.h

test: test.o cmdtree.o cmdtree.h
	$(CC) $(CFLAGS) test.o cmdtree.o -o test

tidy:
	-rm *~ *.o

clean:	tidy
	-rm $(OBJECT)
