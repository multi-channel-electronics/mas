
# Don't make example dir by default, it's not really in our build system
SUB_DIRS = dsp_cmd dsp_status mas_mon mas_param mas_var maslog mce_cmd \
	mce_jam mce_ramp mce_status mux_lock psc_status raw_acq testing

sub-dirs:
	  @for d in ${SUB_DIRS}; do \
			( cd $$d && ${MAKE}; ) || exit 1;\
		done

install:
	@for d in ${SUB_DIRS}; do \
		( cd $$d && ${MAKE} install; ) || exit 1;\
	done
	@echo
	@echo "Make sure you stop maslog_server before re-installing, or"\
		"the install will fail."
	@echo

clean:
	@for d in ${SUB_DIRS}; do \
		( cd $$d && ${MAKE} clean; ) || exit 1;\
  done

.PHONY: sub-dirs clean
