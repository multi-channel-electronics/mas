default: all

BASE := ../..
include $(BASE)/Makefile.children

# Module name
MOD = pymce
CPPFLAGS += $(PYTHON_CPPFLAGS)
LDFLAGS += $(PYTHON_LIBS)

# Source files
SOURCE = $(MOD)/__init__.py $(MOD)/basic.py $(MOD)/compat.py base.c

all: .build

.build: $(SOURCE) _default_setup.py $(LIBDEP)
	-rm -rf build .build   # This is necessary because of external library dependency.
	python setup.py build
	touch .build

install: default
	python setup.py install --install-lib=$(MAS_INSTALL_PYTHON)

_default_setup.py:
	-rm _default_setup.py
	echo lib = \"$(MCELIB) config\" >> $@
	echo inc_dir = \"$(MAS_INCLUDE_F)\" >> $@
	echo lib_dir = \"$(MCELIB_F)\" >> $@

clean:
	-python setup.py clean
	-rm -r build
	-rm _default_setup.py
	make -C $(MOD) clean
