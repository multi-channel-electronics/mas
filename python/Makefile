default: all

BASE := ..
include $(BASE)/Makefile.children

SCRIPTS=check_lock.py noise_setup.py servo.py

all clean:
	( cd mcelib && ${MAKE} $@ )

install: $(SCRIPTS)
	( cd mcelib && ${MAKE} install )
	install -m u+rwx,g+rwx,o+rx --owner=$(MAS_USER) --group=$(MAS_GROUP) \
		 $(SCRIPTS) $(MAS_INSTALL_PYTHON)
