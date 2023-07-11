from __future__ import print_function
# Use this tree's build by default!
from builtins import object
import sys
sys.path.insert(1, '../build/lib.linux-i686-2.6')

# Did you build your edits?  Ask make if target is up-to-date
import os
err = os.system('make -q -C ..')
if err != 0:
    print('.. is out of date!  make?')
    print()

# Utility:

import time
class timer(object):
    def __init__(self):
        self.t0 = time.time()
    def get(self):
        return time.time() - self.t0

