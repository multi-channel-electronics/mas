CC=gcc

INCLUDE += -I../../include -I../../interfaces/dsp_library
LIBRARY += -L../../interfaces/dsp_library -lmcedsp

CFLAGS += $(INCLUDE)

default: dsp_cmd

all: dsp_cmd

dsp_cmd: dsp_cmd.o
	$(CC) $(CFLAGS) $< -o $@ $(LIBRARY)

clean:
	-rm *.a *.o *~ dsp_cmd