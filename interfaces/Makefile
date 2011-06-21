
sub-dirs:
	cd misc && ${MAKE}
	cd mce_library && ${MAKE}
	cd dsp_library && ${MAKE}

clean:
	cd misc && ${MAKE}        clean
	cd dsp_library && ${MAKE} clean
	cd mce_library && ${MAKE} clean

install:
	cd dsp_library && ${MAKE} install
	cd mce_library && ${MAKE} install

.PHONY: sub-dirs
