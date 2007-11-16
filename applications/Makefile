
sub-dirs:
	make -C dsp_cmd
#	make -C masdata
	make -C maslog
	make -C mce_cmd

install:
	make -C dsp_cmd install
#	make -C masdata install
	make -C maslog  install
	make -C mce_cmd install

clean:
	make -C dsp_cmd clean
#	make -C masdata clean
	make -C maslog  clean
	make -C mce_cmd clean

.PHONY: sub-dirs clean
