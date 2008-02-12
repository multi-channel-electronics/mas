#!/bin/bash

if [ "$MAS_ROOT" == "" ]; then
	export MAS_ROOT=/home/mce/mce_script/
fi

#Trailing slashes are mandatory!!

export MAS_BIN=/usr/mce/bin/
export MAS_DATA=/data/cryo/current_data/
export MAS_TEMPLATE=${MAS_ROOT}template/
export MAS_SCRIPT=${MAS_ROOT}script/
export MAS_TEMP=/home/mce/tmp/
export MAS_IDL=${MAS_ROOT}idl_pro/
export PATH=${PATH}:${MAS_BIN}:${MAS_SCRIPT}
