from __future__ import division
from builtins import range
from past.utils import old_div
from builtins import object
from mce import ChannelSet
from mceViewport import mcePlotInput

from numarray import *
from numarray.fft import *

class mceChannelAcq(object):

    def __init__(self, mce):
        self.mce = mce

    def acq(self, col, row, n_frames):
        cs = ChannelSet()
        cs.columns = [col]
        cs.rows = [row]

        i32 = self.mce.read_channel(count=n_frames, channel_set=cs)

        pi = mcePlotInput()
        pi.y = [ i32[i] for i in range(n_frames) ]
        pi.name = 'Column %i, row %i' % (col, row)
        
        return pi


class mceChannelSetAcq(object):

    def __init__(self, mce):
        self.mce = mce

    def acq(self, channel_set, n_frames):
        
        i32 = self.mce.read_channel(count=n_frames, channel_set=channel_set)

        pi = mcePlotInput()
        pi.y = [ i32[i] for i in range(n_frames) ]
        pi.name = 'Column %i, row %i' % (col, row)
        
        return pi


class mceColumnAcq(object):

    def __init__(self, mce):
        self.mce = mce

    def acq(self, col, n_frames):
        cs = ChannelSet()
        cs.columns = [col]
        n_rows = self.mce.read('cc','num_rows_reported',array=False)
        cs.rows = [r for r in range(n_rows)]

        i32 = self.mce.read_frames(n_frames, cs, data_only=True)

        pi = mcePlotInput()
        pi.y = [ i32[f][r] for f in range(n_frames) for r in range(n_rows) ]
        pi.name = 'Column %i, time-ordered' % col

        return pi
    
class mceColumnCorrAcq(object):

    def __init__(self, mce):
        self.mce = mce

    def acq(self, col1, col2, n_frames):
        cs = ChannelSet()
        cs.columns = [col1, col2]
        n_rows = self.mce.read('cc','num_rows_reported',array=False)
        cs.rows = [r for r in range(n_rows)]

        i32 = self.mce.read_frames(n_frames, cs, data_only=True)
        c1 = array( [ i32[f][r*2]
                      for f in range(n_frames)
                      for r in range(n_rows) ] )
        c2 = array( [ i32[f][r*2+1]
                      for f in range(n_frames)
                      for r in range(n_rows) ] )

        f1 = fft(c1 - c1.mean())
        f2 = fft(c2 - c2.mean())
        cc = inverse_fft(f1*conjugate(f2))
        cc = old_div(cc, cc[0])

        pi = mcePlotInput()
        pi.y = cc.real
        pi.name = 'Cross-correlation, column %i v. %i' % (col1,col2)

        return pi
    
