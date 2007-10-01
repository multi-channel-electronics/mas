CC=gcc

# DSPINC=-I../dsp_library -I../driver -I../mce_library
# MCELIB= -lmcecmd -L../mce_library -lexpat -L../libmaslog -lmaslog -lconfig

INTERFACES=../../interfaces

INCLUDE=-I$(INTERFACES)/mce_library -I../../include

LIBRARY=-L$(INTERFACES)/mce_library -lmcecmd \
	-L$(INTERFACES)/log_library -lmaslog \
	-lexpat -lconfig

CFLAGS += $(INCLUDE)

default: mce_cmd

all: mce_cmd

mce_cmd: mce_cmd.o
	$(CC) $(CFLAGS) $< -o $@ $(LIBRARY)

tidy:
	-rm *~ *.o

clean:	tidy
	-rm mce_cmd