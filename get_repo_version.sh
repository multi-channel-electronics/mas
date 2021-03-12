#!/bin/sh

# This script should only ever be called by autoconf while building
# configure!

export LANG=C
export LC_ALL=C

gitrev=$(git rev-parse HEAD 2>/dev/null)

if [ "x$gitrev" = "x" ]; then
  if [-f repo_version.txt ]; then
    cat repo_version.txt
  else
    echo "Unversioned directory"
  fi
else
  echo $gitrev
fi
