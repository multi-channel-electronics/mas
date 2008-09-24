default: all

BASE := ../
include $(BASE)Makefile.children

EXTRA = $(MASMISC_F)socks.o
INCLUDE = -I$(MAS_INCLUDE_F)

CFLAGS += $(INCLUDE) -Wall -fPIC

CC=gcc

# Target name for logger client library
LOGLIB = libmaslog.a


all: $(LOGLIB)

$(LOGLIB): $(EXTRA) libmaslog.o $(MAS_INCLUDE_F)libmaslog.h
	$(AR) rs $@ $(EXTRA) libmaslog.o

# Make the install__libmaslog.a
INSTALL_TARGET=$(LOGLIB)
include $(MAKERULES)Makefile.install_rule_lib

install: install__$(LOGLIB)

tidy:
	-rm *~ *.o

clean:	tidy
	-rm $(LOGLIB)
