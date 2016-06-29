#!/bin/sh

# This script should only ever be called by autoconf while building
# configure!

export LANG=C
export LC_ALL=C

svnvers=`svnversion .`

if [ "x$svnvers" = "xUnversioned directory" -a -f repo_version.txt ]; then
  cat repo_version.txt
else
  echo $svnvers
fi
