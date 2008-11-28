BASE = ../
include $(BASE)Makefile.children

# Normal run-level in Ubuntu is 2.  On FC, it's 5 (X) or 3 (console).
INSTALL_LEVELS=2
INSTALL_TARGET=mas
INSTALL_PRIORITY=99
include $(MAKERULES)Makefile.install_rule_init

INSTALL_TARGET=mas_mknodes
include $(MAKERULES)Makefile.install_rule

install: install__mas install__mas_mknodes
