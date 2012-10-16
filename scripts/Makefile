default: all

BASE = ../
include $(BASE)Makefile.children

# Normal run-level in Ubuntu is 2.  On FC, it's 5 (X) or 3 (console).
INSTALL_LEVELS=2
INSTALL_TARGET=mas
INSTALL_PRIORITY=99
include $(MAKERULES)Makefile.install_rule_init

INSTALL_TARGET=mas_mknodes
include $(MAKERULES)Makefile.install_rule

INSTALL_TARGET=mas_make_card_id
include $(MAKERULES)Makefile.install_rule

all: mas mas_mknodes

install: install__mas install__mas_mknodes install__mas_make_card_id

mas: ../config.status mas.in
	cd .. && $(MAKE) scripts/mas

mas_mknodes: ../config.status mas_mknodes.in
	cd .. && $(MAKE) scripts/mas_mknodes

clean:
