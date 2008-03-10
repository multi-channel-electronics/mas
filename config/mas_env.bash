#!/bin/bash

# To run the mce_scripts from your own checked-out source tree, point
# the MAS_ROOT variable to your checked out copy and then source this
# file.  e.g.:
#
#     export MAS_ROOT="/home/mhasse/mce_script/trunk/"
#     source mas_env.bash
#
# If your .bashrc sources mas_env.bash automatically, with a different
# MAS_ROOT, you probably need to set the $PATH variable by hand as it
# will still preferentially run things from the old directories.

if [ "$MAS_ROOT" == "" ]; then
	export MAS_ROOT=/usr/mce/mce_script
fi

#Trailing slashes are recommended

export MAS_BIN=/usr/mce/bin/
export MAS_TEMP=/tmp/
export MAS_DATA=/data/cryo/current_data/

export MAS_TEMPLATE=${MAS_ROOT}/template/
export MAS_SCRIPT=${MAS_ROOT}/script/
export MAS_TEST_SUITE=${MAS_ROOT}/test_suite/
export MAS_IDL=${MAS_ROOT}/idl_pro/

export PATH=${PATH}:${MAS_BIN}:${MAS_SCRIPT}:${MAS_TEST_SUITE}
