default: all

BASE := ../
include $(BASE)Makefile.children

EXTRA = $(MASMISC_F)socks.o
INCLUDE = -I$(MASMISC_F)

CFLAGS += $(INCLUDE) -Wall

CC=gcc

# Target name for logger client library
LOGLIB = libmaslog.a

SOCK = ../misc/socks.o
INCLUDE = -I ../misc

CC = gcc

CFLAGS += $(INCLUDE)

all: $(LOGLIB)

$(LOGLIB): $(EXTRA) libmaslog.o libmaslog.h
	$(AR) rs $@ $(EXTRA) libmaslog.o

# Make the install__libmaslog.a and install__libmaslog.h rules
INSTALL_TARGET=$(LOGLIB)
include $(MAKERULES)Makefile.install_rule_lib

INSTALL_TARGET=libmaslog.h
include $(MAKERULES)Makefile.install_rule_inc


install: install__$(LOGLIB) install__libmaslog.h

tidy:
	-rm *~ *.o

clean:	tidy
	-rm $(LOGLIB)
