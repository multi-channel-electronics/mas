default: all

BASE := ..
include $(BASE)/Makefile.children

# Normal run-level in Ubuntu is 2.  On FC, it's 5 (X) or 3 (console).
INSTALL_LEVELS=2
INSTALL_TARGET=mas
INSTALL_PRIORITY=99
include $(MAKERULES)/Makefile.install_rule_init

INSTALL_TARGET=mas_mknodes
include $(MAKERULES)/Makefile.install_rule

INSTALL_TARGET=mas_make_card_id
include $(MAKERULES)/Makefile.install_rule

all: mas mas_mknodes mas_make_card_id 91-mas.rules

install: install__mas install__mas_mknodes install__mas_make_card_id

mas: ../config.status mas.in
	cd .. && $(MAKE) scripts/mas

mas_make_card_id: ../config.status mas_make_card_id.in
	cd .. && $(MAKE) scripts/mas_make_card_id

mas_mknodes: ../config.status mas_mknodes.in
	cd .. && $(MAKE) scripts/mas_mknodes

91-mas.rules: ../config.status 91-mas.rules.in
	cd .. && $(MAKE) scripts/91-mas.rules

clean:
	rm -f mas mas_make_card_id mas_mknodes 91-mas.rules
