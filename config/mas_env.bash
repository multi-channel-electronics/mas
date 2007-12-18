#!/bin/bash

if [ "$MAS_ROOT" == "" ]; then
	export MAS_ROOT=/home/mce/
fi

#Trailing slashes are mandatory!!

export MAS_BIN=/usr/mce/bin/
export MAS_DATA=/data/cryo/current_data/
export MAS_TEMPLATE=${MAS_ROOT}/mce_script/clover/template/
export MAS_SCRIPT=${MAS_ROOT}/mce_script/clover/script/
export MAS_TEMP=${MAS_ROOT}/mce_script/tmp/
export PATH=${PATH}:${MAS_BIN}:${MAS_SCRIPT}
