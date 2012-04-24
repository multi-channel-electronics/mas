
sub-dirs:
	make -C misc
	make -C dsp_library
	make -C mce_library

clean:
	make -C misc        clean
	make -C dsp_library clean
	make -C mce_library clean

install:
	make -C dsp_library install
	make -C mce_library install

.PHONY: sub-dirs
