default: all

BASE := ../
include $(BASE)Makefile.children

EXTERN_OBJ = $(MASMISC_F)socks.o

# Target name for logger client library
LIBNAME = lib$(LOGLIB).a

all: $(LIBNAME)

$(LIBNAME): $(EXTERN_OBJ) libmaslog.o $(MAS_INCLUDE_F)libmaslog.h
	$(AR) rs $@ $(EXTERN_OBJ) libmaslog.o

# Make the install__libmaslog.a rule
INSTALL_TARGET=$(LIBNAME)
include $(MAKERULES)Makefile.install_rule_lib

install: install__$(LIBNAME)

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f $(LIBNAME)
