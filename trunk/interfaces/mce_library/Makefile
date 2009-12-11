default: all

BASE := ../
include $(BASE)Makefile.children

LIBNAME = lib$(MCELIB).a

OBJECTS = context.o errors.o \
	cmd.o packet.o virtual.o \
	data.o acq.o data_thread.o files.o frame_manip.o \
	dirfile.o config.o data_mode.o

HEADERS = data_thread.h virtual.h

REPO_VERSION=$(shell svnversion)

all: $(LIBNAME)

INSTALL_TARGET=$(LIBNAME)
include $(MAKERULES)Makefile.install_rule_lib

install: install__$(LIBNAME)

$(LIBNAME) : $(OBJECTS) version.h
	$(AR) rs $@ $(OBJECTS)

$(OBJECTS): $(HEADERS)

data_mode.o: data_mode.c data_mode.def

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