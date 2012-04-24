
# Don't make example dir by default, it's not really in our build system
SUB_DIRS = dsp_cmd dsp_status mas_mon mas_param mas_var maslog mce_cmd \
	mce_jam mce_ramp mce_status mux_lock psc_status raw_acq testing

sub-dirs:
	$(foreach d, $(SUB_DIRS), make -C $(d); )

install:
	$(foreach d, $(SUB_DIRS), make -C $(d) install; )
	@echo
	@echo "Make sure you stop maslog_server before re-installing, or"\
		"the install will fail."
	@echo

clean:
	$(foreach d, $(SUB_DIRS), make -C $(d) clean; )

.PHONY: sub-dirs clean
