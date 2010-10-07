
# Don't make example dir by default, it's not really in our build system
SUB_DIRS = dsp_cmd mas_config mas_mon mas_param maslog mce_cmd mce_status \
					 mce_ramp mux_lock psc_status raw_acq testing

sub-dirs:
	@for d in ${SUB_DIRS}; do		\
		make -C $$d || exit 1;		\
	done

install:
	@for d in ${SUB_DIRS}; do		\
		make -C $$d install || exit 1;	\
	done
	@echo -e "\n Make sure you stop maslog_server before re-installing, or "\
		"the install will fail.\n"

clean:
	@for d in ${SUB_DIRS}; do		\
		make -C $$d clean || exit 1;	\
	done

.PHONY: sub-dirs clean
