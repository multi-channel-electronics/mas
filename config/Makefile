default: all

BASE := ../
include $(BASE)Makefile.children

TARGETS=$(shell ./makefile.py --targets)
DEPS=$(shell ./makefile.py --dependencies)

OTHER_ARGS=--define RC_40E

all: $(TARGETS)

$(TARGETS) : $(DEPS)
	./makefile.py $(OTHER_ARGS)

install:
	install --group=$(MAS_GROUP) --mode=775 -d /etc/mce
	@for V in v4 v5; do \
	  install --group=$(MAS_GROUP) --mode=775 -d /etc/mce/$$V ;\
	  install --group=$(MAS_GROUP) --mode=664 -t /etc/mce/$$V $$V/* ;\
	done
	test -h /etc/mce/mce.cfg || ln -s v5/mce_v1.cfg /etc/mce/mce.cfg
	install --group=$(MAS_GROUP) --mode=664 mas.cfg /etc/mce/

clean:
	rm $(TARGETS)

