default: all

BASE := ../
include $(BASE)Makefile.children

LIB     = mcecmd
LIBNAME = lib$(LIB).a

DSP_DIR = ../driver

OBJECTS = context.o errors.o \
	cmd.o packet.o virtual.o \
	data.o acq.o data_thread.o files.o frame_manip.o \
	dirfile.o config.o

HEADERS = data_thread.h virtual.h

INCLUDE += -I. -I$(MAS_INCLUDE_F) -I$(LOGLIB_F) -I$(DSP_DIR)

CFLAGS += -O2 -Wall $(INCLUDE) -fPIC

CC = gcc

REPO_VERSION=$(shell svnversion)

all: $(LIBNAME)

INSTALL_TARGET=$(LIBNAME)
include $(MAKERULES)Makefile.install_rule_lib

install: install__$(LIBNAME)

$(LIBNAME) : $(OBJECTS) version.h
	$(AR) rs $@ $(OBJECTS)

$(OBJECTS): $(HEADERS)

context.o: version.h

version.h: version.h.template FORCE
	grep -v '^\/\*-' version.h.template | \
		sed 's/\$$SVN\./$(REPO_VERSION)/g' | \
		sed 's:\$$REPO\.:$(BRANCH_ID):g' > version.h

docs:
	doxygen

clean: tidy
	-rm $(LIBNAME) version.h

tidy:
	-rm -f core.* *~ *.c~ *.h~ core.* *.o #*#

.PHONY: FORCE