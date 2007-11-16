default: all

BASE := ../
include $(BASE)Makefile.children

INCLUDE=-I$(DSPLIB_F) -I$(MAS_INCLUDE_F) -I$(MASMISC_F)

LIBRARY=-L$(DSPLIB_F) -l$(DSPLIB) \
	-L$(LOGLIB_F) -l$(LOGLIB) \
	$(MASMISC_F)cmdtree.o \
	-lconfig 

CFLAGS += $(INCLUDE) -Wall

CC=gcc

#targets

all: dsp_cmd

dsp_cmd: dsp_cmd.o
	$(CC) $(CFLAGS) $< -o $@ $(LIBRARY)

tidy:
	rm dsp_cmd
clean: tidy
	-rm *.o *~

# Make the install__dsp_cmd rule
INSTALL_TARGET=dsp_cmd
include $(MAKERULES)Makefile.install_rule

install: install__dsp_cmd
