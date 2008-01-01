
sub-dirs:
	make -C dsp_cmd
	make -C mce_cmd
	make -C mce_status
	make -C maslog

install:
	make -C dsp_cmd    install
	make -C mce_cmd    install
	make -C mce_status install
	echo -e "\n Make sure you stop maslog_server before re-installing, or "\
		"the install will fail.\n"
	make -C maslog     install

clean:
	make -C dsp_cmd    clean
	make -C mce_cmd    clean
	make -C mce_status clean
	make -C maslog     clean

.PHONY: sub-dirs clean
