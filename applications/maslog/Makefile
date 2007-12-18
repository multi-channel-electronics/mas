default: all

include ../Makefile.children

LIBRARY = -L$(LOGLIB_F) -l$(LOGLIB) \
	-L/usr/local/lib -lconfig

SOCK = $(MASMISC_F)/socks.o

SOURCE = logger.c main.c init.c
HEADER = logger.h init.h

INCLUDE += -I$(MASMISC_F) -I$(LOGLIB_F)

CFLAGS += -Wall $(INCLUDE)

CC=gcc

# Target name for logger application

LOGAPP := maslog_server
CLIENT := maslog_now
SCRIPT := maslog

all: $(LOGAPP) $(CLIENT)

$(LOGAPP): $(SOURCE) $(HEADER)
	$(CC) $(CFLAGS) $(SOURCE) -o$@ $(SOCK) $(LIBRARY)

$(CLIENT): client.c
	$(CC) $(CFLAGS) $< -o$@ $(LIBRARY)

tidy:
	-rm *~ *.o

clean:	tidy
	-rm $(LOGAPP)
	-rm $(CLIENT)

#Make install rules for app, client, wrapper
INSTALL_TARGET := $(LOGAPP)
include $(MAKERULES)Makefile.install_rule

INSTALL_TARGET := $(CLIENT)
include $(MAKERULES)Makefile.install_rule

INSTALL_TARGET := $(SCRIPT)
include $(MAKERULES)Makefile.install_rule

install: install__$(LOGAPP) install__$(CLIENT) install__$(SCRIPT)