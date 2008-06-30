
# Don't make example dir by default, it's not really in our build system
SUB_DIRS = dsp_cmd mce_cmd mce_status maslog mas_param mux_lock mce_ramp \
	 psc_status raw_acq mas_mon

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
