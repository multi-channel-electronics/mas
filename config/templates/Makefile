DEFS_ALL = 
OPTIONS = -P -nostdinc -I ../ $(DEFS_ALL)

all: mce_v1.cfg mce_v2.cfg mce_v1_bac.cfg mce_v2_bac.cfg mce_v2_16tes.cfg

clean:
	-rm *cfg

%.cfg: %.template
	cpp $(OPTIONS) $< $@ || rm $@


mce_v1.cfg: master.template
	cpp $(OPTIONS) $< $@ || rm $@

mce_v2.cfg: master.template
	cpp $(OPTIONS) -DSUBRACK_SMALL $< $@ || rm $@

mce_v1_bac.cfg: master.template
	cpp $(OPTIONS) -DBIASING_AC $< $@ || rm $@

mce_v2_bac.cfg: master.template
	cpp $(OPTIONS) -DSUBRACK_SMALL -DBIASING_AC $< $@ || rm $@

mce_v2_16tes.cfg: master.template
	cpp $(OPTIONS) -DSUBRACK_SMALL -DTES_16 $< $@ || rm $@

