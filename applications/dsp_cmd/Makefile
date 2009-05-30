default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version

LIBRARY=$(MAS_LIB) $(MASMISC_F)cmdtree.o

CFLAGS += -DVERSION_STRING="$(REPO_VER)"

#targets

all: dsp_cmd

dsp_cmd: dsp_cmd.o
	$(CC) $(CFLAGS) $< -o $@ $(LIBRARY)

tidy:
	-rm dsp_cmd

clean: tidy
	-rm *.o *~

# Make the install__dsp_cmd rule
INSTALL_TARGET=dsp_cmd
include $(MAKERULES)Makefile.install_rule

install: install__dsp_cmd
