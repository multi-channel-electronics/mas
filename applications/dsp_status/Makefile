default: all

BASE := ../
include $(BASE)Makefile.children

VER_TARGET := .
include $(MAKERULES)Makefile.version
DEFS = -DVERSION_STRING="$(REPO_VER)"

# targets

all: dsp_status

dsp_status:

tidy:
	-rm *~ *.pyc

clean:	tidy


# Make the install__dsp_status rule
INSTALL_TARGET=dsp_status
include $(MAKERULES)Makefile.install_rule

install: install__dsp_status
