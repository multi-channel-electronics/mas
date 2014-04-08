default: all

include ../Makefile.children

LIBRARY = $(MCE_LIBS)

SOURCE = logger.c main.c init.c
HEADER = logger.h init.h $(LIBHEADERS)

CFLAGS += $(DEFS)

# Target name for logger application

LOGAPP := maslog_server
CLIENT := maslog_now
SCRIPT := maslog

all: $(LOGAPP) $(CLIENT)

$(LOGAPP): $(SOURCE) $(HEADER) $(SOCK) $(LIBDEP)
	$(CC) $(CFLAGS) $(SOURCE) -o $@ $(SOCK) $(LIBRARY)

$(CLIENT): client.c $(LIBDEP)
	$(CC) $(CFLAGS) $< -o $@ $(LIBRARY)

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f $(LOGAPP)
	rm -f $(CLIENT)

#Make install rules for app, client, wrapper
INSTALL_TARGET := $(LOGAPP)
include $(MAKERULES)Makefile.install_rule

INSTALL_TARGET := $(CLIENT)
include $(MAKERULES)Makefile.install_rule

INSTALL_TARGET := $(SCRIPT)
include $(MAKERULES)Makefile.install_rule

install: install__$(LOGAPP) install__$(CLIENT) install__$(SCRIPT)
