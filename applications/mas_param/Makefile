default: all

BASE := ..
include $(BASE)/Makefile.children

VER_TARGET := .
include $(MAKERULES)/Makefile.version
DEFS = -DVERSION_STRING="$(REPO_VER)"

LIBRARY=$(MCE_LIBS) $(CONFIG_LIBS)

CFLAGS += $(DEFS)

OBJECTS = main.o \
					bash.o \
					crawl.o \
					csh.o \
					get.o \
					idl.o \
					info.o \
					masconfig.o \
					options.o

HEADERS = mas_param.h get.h crawl.h bash.h csh.h idl.h ../../defaults/config.h \
					$(LIBHEADERS)

# targets

all: mas_param

mas_param: $(OBJECTS) $(LIBDEP)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LIBRARY)

$(OBJECTS): $(HEADERS)

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f mas_param

# Make the install__mas_param rule
INSTALL_TARGET=mas_param
include $(MAKERULES)/Makefile.install_rule

install: install__mas_param

