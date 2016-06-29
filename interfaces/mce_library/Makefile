default: all

BASE := ..
include $(BASE)/Makefile.children

LIBNAME = lib$(MCELIB)

# the following definitions may only used in the no-mas.cfg fallback code in
# context.c and as the default location for mas.cfg in defaults.c.  Using them
# elsewhere will break path abstraction.
CPPFLAGS += -DMAS_ETCDIR="\"${MAS_ETCDIR}\"" \
						-DMAS_CONFDIR="\"${MAS_CONFIDR}\"" \
						-DMAS_JAMDIR="\"${MAS_JAMDIR}\"" \
						-DMAS_ROOTDIR="\"${MAS_ROOTDIR}\"" \
						-DMAS_TEMPDIR="\"${MAS_TEMPDIR}\"" \
						-DMAS_DATAROOT="\"${MAS_DATAROOT}\"" \
						-DMAS_DATADIR="\"${MAS_DATADIR}\""

OBJECTS = \
					acq.o \
					cmd.o \
					cmdtree.o \
					config.o \
					context.o \
					data.o \
					data_mode.o \
					data_thread.o \
					defaults.o \
					dirfile.o \
					dsp_errors.o \
					dsp_library.o \
					errors.o \
					files.o \
					frame_manip.o \
					libmaslog.o \
					manip.o \
					multisync.o \
					packet.o \
					socks.o \
					virtual.o

HEADERS = context.h data_thread.h virtual.h manip.h ../../defaults/config.h \
					$(LIBHEADERS)

all: $(LIBNAME)$(LIB_SUFFIX)

INSTALL_TARGET=$(LIBNAME)
include $(MAKERULES)/Makefile.install_rule_lib

install: install__$(LIBNAME)

$(LIBNAME).so : $(OBJECTS) version.h
	$(LD) -shared -o $@ $(OBJECTS) -lc -lpthread $(CONFIG_LIBS)

$(LIBNAME).a : $(OBJECTS) version.h
	$(AR) rs $@ $(OBJECTS)

$(OBJECTS): $(HEADERS)

data_mode.o: data_mode.c data_mode.def

context.o: version.h

docs:
	doxygen

clean: tidy
	rm -f $(LIBNAME).a $(LIBNAME).so

tidy:
	rm -f core.* *~ *.c~ *.h~ core.* *.o #*#

.PHONY: FORCE
