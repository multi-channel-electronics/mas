default: all

BASE := ../
include $(BASE)Makefile.children

ifeq ($(wildcard /usr/include/python2.5/Python.h),/usr/include/python2.5/Python.h)
PYTHON_INC := /usr/include/python2.5/
else
PYTHON_INC := /usr/include/python2.4/
endif

INCLUDE=-I$(MAS_INCLUDE_F) -I$(PYTHON_INC)

LIBRARY=-L$(MCELIB_F) -l$(MCELIB) \
	-L$(LOGLIB_F) -l$(LOGLIB) \
	-lconfig

include $(MAKERULES)Makefile.defaults
DEFS += $(CONFIGFILE_DEFS)

CFLAGS += $(INCLUDE) -Wall $(DEFS) -fPIC
CC=gcc

all: _mce_library.so

_mce_library.so: mce_library_wrap.o
	ld -shared -fPIC -o $@ $^ $(LIBRARY)

mce_library_wrap.o: mce_library_wrap.c

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
