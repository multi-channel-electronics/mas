CC=gcc

LIB=-lmcecmd -lconfig -lmaslog -lpthread

CFLAGS=-O2 -Wall

TARGETS=sq1servo sq2servo

INSTALL_DIR=$(MAS_BIN)

all: $(TARGETS)

install: $(TARGETS)
	cp $(TARGETS) $(INSTALL_DIR)
	cd $(INSTALL_DIR) ; chmod g+s $(TARGETS)

sq2servo: sq2servo.o servo.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIB) $(INC)

sq1servo: sq1servo.o servo.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIB) $(INC)

clean:
	rm *.o
	rm $(TARGETS)
