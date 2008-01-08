#!/bin/tcsh

if ( "$MAS_ROOT" == "" ) then
    setenv MAS_ROOT /home/mce/
endif

#Trailing slashes are mandatory!!

setenv MAS_BIN /usr/mce/bin/
setenv MAS_DATA /data/cryo/current_data/
setenv MAS_TEMPLATE ${MAS_ROOT}mce_script/clover/template/
setenv MAS_SCRIPT ${MAS_ROOT}mce_script/clover/script/
setenv MAS_TEMP ${MAS_ROOT}/mce_script/clover/tmp/
setenv MAS_IDL ${MAS_ROOT}idl_pro/
setenv PATH ${PATH}:${MAS_BIN}:${MAS_SCRIPT}
