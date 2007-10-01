TARGET = socks.o

OBJECT = $(TARGET)
SOURCE = socks.c
HEADER = socks.h

CC = gcc

default: all

all: $(TARGET)

$(OBJECT): $(SOURCE) $(HEADER)
	$(CC) $(CFLAGS) -c -o $(OBJECT) $(SOURCE)

tidy:
	-rm *~ *.o

clean:	tidy
	-rm $(OBJECT)
