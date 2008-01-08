default: all

CC=gcc

LIB=-lmcecmd -lconfig -lmaslog -lpthread

# -std=c99 lets us define vars anywhere...
CFLAGS=-Wall -std=c99

all: example example_data

example: example.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIB) $(INC)

example_data: example_data.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIB) $(INC)

tidy:
	-rm *.o *~ #*

clean: tidy
	-rm example example_data
