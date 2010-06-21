from numpy import *

from mce import mce

scales = {0: 1, 1: 2**12, 10: 2**4*1218.}


m = mce()
n_frames = 100
dm = m.read('rc1', 'data_mode')[0]
data = array(m.read_frames(n_frames, data_only=True)).transpose() \
    / scales[dm]

for i in range(30):
    print '%4i %10.2f %10.2f' % (i, data[i].mean(), data[i].std())


