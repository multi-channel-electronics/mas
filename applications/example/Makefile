# Note that this lives outside the MAS build system!  It can survive out of tree.

default: all

CC=gcc
LIB=-L/usr/local/lib -lmcecmd -lconfig -lmaslog -lpthread

# -std=c99 lets us define vars anywhere...
CFLAGS=-Wall -std=c99 -I/usr/local/include

all: example example_data

example: example.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIB) $(INC)

example_data: example_data.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIB) $(INC)

install:
	@echo "The examples don't install!"

tidy:
	rm -f *.o *~

clean: tidy
	rm -f example example_data
