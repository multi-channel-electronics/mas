
# Don't make example dir by default, it's not really in our build system
SUB_DIRS = dsp_cmd mce_cmd mce_status maslog mas_param mux_lock mce_ramp

sub-dirs:
	@for d in ${SUB_DIRS}; do	\
		make -C $$d;		\
	done

install:
	@for d in ${SUB_DIRS}; do	\
		make -C $$d install;	\
	done
	@echo -e "\n Make sure you stop maslog_server before re-installing, or "\
		"the install will fail.\n"

clean:
	@for d in ${SUB_DIRS}; do	\
		make -C $$d clean;	\
	done

.PHONY: sub-dirs clean
