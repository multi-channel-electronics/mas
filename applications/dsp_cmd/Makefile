default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version

LIBRARY=$(DSP_LIBS) $(MCE_LIBS) $(MASMISC_F)cmdtree.o

CFLAGS += -DVERSION_STRING="$(REPO_VER)"

#targets

all: dsp_cmd

dsp_cmd: dsp_cmd.o
	$(CC) $(CFLAGS) $< -o $@ $(LIBRARY)

tidy:
	rm -f dsp_cmd

clean: tidy
	rm -f *.o *~

# Make the install__dsp_cmd rule
INSTALL_TARGET=dsp_cmd
include $(MAKERULES)Makefile.install_rule

install: install__dsp_cmd
