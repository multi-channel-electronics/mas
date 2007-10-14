CC=gcc

MISC = ../../interfaces/misc/
DSP = ../../interfaces/dsp_library/

INCLUDE += -I../../include -I$(DSP) -I$(MISC)
LIBRARY += -L$(DSP) -lmcedsp $(MISC)cmdtree.o

CFLAGS += -Wall $(INCLUDE)

default: dsp_cmd

all: dsp_cmd

dsp_cmd: dsp_cmd.o
	$(CC) $(CFLAGS) $< -o $@ $(LIBRARY)

clean:
	-rm *.o *~ dsp_cmd