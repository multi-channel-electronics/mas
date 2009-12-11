default: all

include ../Makefile.children

LIBRARY = $(MAS_LIB)
SOCK = $(MASMISC_F)/socks.o

SOURCE = logger.c main.c init.c
HEADER = logger.h init.h

include $(MAKERULES)Makefile.defaults
DEFS += $(CONFIGFILE_DEFS)

CFLAGS += $(DEFS)

# Target name for logger application

LOGAPP := maslog_server
CLIENT := maslog_now
SCRIPT := maslog

all: $(LOGAPP) $(CLIENT)

$(LOGAPP): $(SOURCE) $(HEADER) $(SOCK)
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