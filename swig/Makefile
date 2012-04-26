default: all

BASE := ../
include $(BASE)Makefile.children

INCLUDE=-I$(MAS_INCLUDE_F) -I$(PYTHON_INC)

LIBRARY=-L$(MCELIB_F) -l$(MCELIB) \
	-L$(LOGLIB_F) -l$(LOGLIB) \
	-lconfig

CFLAGS += $(INCLUDE) -Wall $(DEFS) -fPIC -fno-stack-protector -fno-strict-aliasing
CC=gcc

all: _mce_library.so

_mce_library.so: mce_library_wrap.o
	ld -shared -fPIC -fno-stack-protector -o $@ $^ $(LIBRARY)

mce_library_wrap.o: mce_library_wrap.c ../defaults/config.h

mce_library_wrap.c: mce_library.i $(LIBHEADERS)
	swig -python mce_library.i

tidy:
	rm -f *.o *~ *_wrap.c

clean: tidy
	rm -f *.pyc mce_library.py _mce_library.so

install: install___mce_library.so install__mce_library.py install__mce.py

INSTALL_TARGET := _mce_library.so
include $(MAKERULES)/Makefile.install_rule_swig
INSTALL_TARGET := mce_library.py
include $(MAKERULES)/Makefile.install_rule_swig
INSTALL_TARGET := mce.py
include $(MAKERULES)/Makefile.install_rule_swig
