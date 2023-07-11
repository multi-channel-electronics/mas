from __future__ import print_function
import sys
sys.path.append(sys.path[0] + '/..')

from mce_library import *

c = mcelib_create()
mcecmd_open(c, "/dev/mce_cmd0")
mcedata_open(c, "/dev/mce_data0")
mceconfig_open(c, "/etc/mce/mce.cfg", "hardware")

p = mce_param_t()
mcecmd_load_param(c, p, "cc", "fw_rev")

d = u32array(32)
mcecmd_read_block(c, p, 1, d.cast())
print("%x" % int(d[0]))
