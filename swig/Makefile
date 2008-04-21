default: all

BASE := ../
include $(BASE)Makefile.children

PYTHON_INC=/usr/include/python2.5/

INCLUDE=-I$(MAS_INCLUDE_F) -I$(PYTHON_INC)

LIBRARY=-L$(MCELIB_F) -l$(MCELIB) \
	-L$(LOGLIB_F) -l$(LOGLIB) \
	-lconfig

include $(MAKERULES)Makefile.defaults
DEFS += $(CONFIGFILE_DEFS)

CFLAGS += $(INCLUDE) -Wall $(DEFS)
CC=gcc

all: _mce_library.so

_mce_library.so: mce_library_wrap.o
	ld -shared -o $@ $^ $(LIBRARY)

mce_library_wrap.o: mce_library_wrap.c

#	gcc -c -I$(PYTHON_INC) $^

mce_library_wrap.c: mce_library.i
	swig -python $^

tidy:
	-rm *.o *~ *_wrap.c

clean: tidy
	-rm *.pyc mce_library.py _mce_library.so

install: install___mce_library.so install__mce_library.py install__mce.py

INSTALL_TARGET := _mce_library.so
include $(MAKERULES)/Makefile.install_rule_swig
INSTALL_TARGET := mce_library.py
include $(MAKERULES)/Makefile.install_rule_swig
INSTALL_TARGET := mce.py
include $(MAKERULES)/Makefile.install_rule_swig