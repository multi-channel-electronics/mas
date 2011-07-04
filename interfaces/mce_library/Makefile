default: all

BASE := ../
include $(BASE)Makefile.children

LIBNAME = lib$(MCELIB)

OBJECTS = \
					acq.o \
					cmd.o \
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
					eth.o \
					eth_filter.o \
					files.o \
					frame_manip.o \
					libmaslog.o \
					net.o \
					packet.o \
					sdsu.o \
					socks.o \
					virtual.o

HEADERS = data_thread.h virtual.h context.h sdsu.h net.h eth.h \
					../../defaults/config.h \
					../../include/libmaslog.h ../../include/masconfig.h \
					../../include/mce_library.h ../../include/mcecmd.h \
					../../include/mceconfig.h ../../include/mcedata.h \
					../../include/mcedsp.h ../../include/mcenetd.h \
					../../include/mce/acq.h ../../include/mce/data_frame.h \
					../../include/mce/data_ioctl.h ../../include/mce/data_mode.h \
					../../include/mce/defaults.h ../../include/mce/dsp.h \
					../../include/mce/dsp_errors.h ../../include/mce/dsp_ioctl.h \
					../../include/mce/frame.h ../../include/mce/mce_errors.h \
					../../include/mce/mce_ioctl.h ../../include/mce/socks.h \
					../../include/mce/types.h

REPO_VERSION=$(shell svnversion)

all: $(LIBNAME)$(LIB_SUFFIX)

INSTALL_TARGET=$(LIBNAME)
include $(MAKERULES)Makefile.install_rule_lib

install: install__$(LIBNAME)

$(LIBNAME).so : $(OBJECTS) version.h
	$(LD) -shared -o $@ $(OBJECTS) -lc -lpthread $(CONFIG_LIBS)

$(LIBNAME).a : $(OBJECTS) version.h
	$(AR) rs $@ $(OBJECTS)

$(OBJECTS): $(HEADERS)

data_mode.o: data_mode.c data_mode.def

context.o: version.h

version.h: .version-stamp
	if test ! -f $@; then \
		rm -f $<; \
		$(MAKE) $<; \
		fi

.version-stamp: version.h.template FORCE
	grep -v '^\/\*-' version.h.template | \
		sed 's/\$$SVN\./$(REPO_VERSION)/g' | \
		sed 's:\$$REPO\.:$(BRANCH_ID):g' > version.h.new
	if test -e version.h && diff -q version.h version.h.new; then \
		echo "version.h is unchanged."; \
		rm -f version.h.new; \
		else \
		mv -f version.h.new version.h; \
		fi
	touch $@

docs:
	doxygen

clean: tidy
	rm -f $(LIBNAME).a $(LIBNAME).so .version-stamp version.h

tidy:
	rm -f core.* *~ *.c~ *.h~ core.* *.o #*#

.PHONY: FORCE
