default: all

TARGET=mce_jam

BASE := ..
include $(BASE)/Makefile.children

LIB=$(MCE_LIBS)
CFLAGS += -Wall -Wno-unused-result -std=c99

OBJS = jamstub.o jamexec.o jamnote.o jamcrc.o jamsym.o jamstack.o jamheap.o \
	jamarray.o jamcomp.o jamjtag.o jamutil.o jamexp.o jam_mce.o

all: $(TARGET)

.c.o:
	gcc $(CFLAGS) -c -I. $<

$(TARGET) : $(OBJS) $(LIBDEP)
	gcc $(OBJS) -o $(TARGET) $(LIB)

jamstub.o: jamstub.c jamport.h jamexprt.h jam_mce.h ../../defaults/config.h

jam_mce.o: jam_mce.c jam_mce.h ../../defaults/config.h $(LIBHEADERS)

jamexec.o: jamexec.c jamport.h jamexprt.h jamdefs.h jamexec.h jamutil.h \
	jamexp.h jamsym.h jamstack.h jamheap.h jamarray.h jamjtag.h

jamnote.o: jamnote.c jamexprt.h jamdefs.h jamexec.h jamutil.h

jamcrc.o: jamcrc.c jamexprt.h jamdefs.h jamexec.h jamutil.h

jamsym.o: jamsym.c jamexprt.h jamdefs.h jamsym.h jamheap.h jamutil.h

jamstack.o: jamstack.c jamexprt.h jamdefs.h jamutil.h jamsym.h jamstack.h

jamheap.o: jamheap.c jamport.h jamexprt.h jamdefs.h jamsym.h jamstack.h \
	jamheap.h jamutil.h

jamarray.o: jamarray.c jamexprt.h jamdefs.h jamexec.h jamexp.h jamsym.h \
	jamstack.h jamheap.h jamutil.h jamcomp.h jamarray.h

jamcomp.o: jamcomp.c jamdefs.h jamcomp.h

jamjtag.o: jamjtag.c jamexprt.h jamdefs.h jamsym.h jamutil.h jamjtag.h

jamutil.o: jamutil.c jamutil.h

jamexp.o: jamexp.c jamexprt.h jamdefs.h jamexp.h jamsym.h jamheap.h \
	jamarray.h jamutil.h jamytab.h

tidy:
	rm -f *~ *.o

clean:	tidy
	rm -f mce_jam

# Make the install__mae_jam rule
INSTALL_TARGET=$(TARGET)
include $(MAKERULES)/Makefile.install_rule

INSTALL_TARGET=sof2jam
include $(MAKERULES)/Makefile.install_rule

install: install__mce_jam install__sof2jam
