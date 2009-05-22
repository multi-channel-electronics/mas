DEFS_ALL = 

TEMPLATES=templates/

OPTIONS = -P -nostdinc -I ./ $(DEFS_ALL)

DEPS = $(TEMPLATES)master.template

all: mce_v1.cfg mce_v2.cfg mce_v1_bac.cfg mce_v2_bac.cfg mce_v2_16tes.cfg

clean:
	-rm *cfg

# Rule for general template -> cfg construction
%.cfg: %.template
	cpp $(OPTIONS) $< $@ || rm $@


# Targets based on the master template
mce_v1.cfg: $(DEPS)
	cpp $(OPTIONS) $< $@ || rm $@

mce_v2.cfg: $(DEPS)
	cpp $(OPTIONS) -DSUBRACK_SMALL $< $@ || rm $@

mce_v1_bac.cfg: $(DEPS)
	cpp $(OPTIONS) -DBIASING_AC $< $@ || rm $@

mce_v2_bac.cfg: $(DEPS)
	cpp $(OPTIONS) -DSUBRACK_SMALL -DBIASING_AC $< $@ || rm $@

mce_v2_16tes.cfg: $(DEPS)
	cpp $(OPTIONS) -DSUBRACK_SMALL -DTES_16 $< $@ || rm $@

