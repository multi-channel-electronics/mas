from __future__ import division
from __future__ import print_function
from builtins import zip
from builtins import range
from past.utils import old_div
from builtins import object
from mce import *

class Ramp(object):
    def __init__(self, this_mce, card, param, param_offset=0, param_count=8,
                 start = 0, count = 1, step = 0. ):
        self.card = card
        self.param = param
        self.param_offset = param_offset
        self.param_count = param_count
        self.start = start
        self.count = count
        self.step = step
        self.i = 0
        self.mce = this_mce

    def Reset(self):
        self.i = 0

    def IsDone(self):
        return (self.i >= self.count)

    def WriteNext(self):
        if self.IsDone():
            return False

        self.mce.write(self.card, self.param,
                       [self.start + self.step*self.count] * self.param_count, \
                           offset = self.param_offset)
        self.i = self.i + 1
        return True

class MCEStage(object):
    SQ1_fb = 1
    SQ2_fb = 2
    SA_fb = 3
    SQ1_bias = 11
    SQ2_bias = 12
    SA_bias = 13

class ServoException(Exception):
    """
    Exception raised by servo when it cannot satisfy a request.
    """
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)



class ColumnServo(object):
    """
    Class for servoing MCE parameters on a per-column basis.  Class
    handles frame acquisition and manipulation of SA, SQ2, or SQ1
    feedback signal.
    """

    def __init__(self, this_mce, stage, gain=0.1, gain_set=None, columns=None,
                 start = None, rows=None, ramp=None, targets=None):
        self.mce = this_mce
        self.gain = gain
        self.param = 'fb'
        if stage == MCEStage.SA_fb:
            self.card = 'sa'
        elif stage == MCEStage.SQ2_fb:
            self.card = 'sq2'
        elif stage == MCEStage.SQ1_fb:
            self.card = 'sq1'
        else:
            raise ServoException('unknown target stage')

        if columns == None:
            self.columns = [ i for i in range(len(self.mce.read(self.card, self.param))) ]
        else:
            self.columns = columns

        if gain_set == None:
            gain_set = [ gain ] * len(self.columns)
        else:
            if len(gain_set) != len(columns):
                raise ServoException('len(gain_list) != n_columns')

        # Get rcs based on columns
        rcs = [ i for i in range(4) if [ old_div(c,8) for c in self.columns ].count(i) > 0 ]
        if len(rcs) > 1:
            self.channel_set = ChannelSet('rcs')
        else:
            self.channel_set = ChannelSet('rc%i' % (rcs[0]+1))
    
        self.start = start
        self.values = None
        if rows == None:
            rows = [0] * len(self.columns)
        if targets == None:
            targets = [0] * len(self.columns)
        self.rows = rows
        self.targets = targets

        cs = self.channel_set.columns_span()
        self.data_offsets = [rows[i]*(cs[1]-cs[0]+1) + c-cs[0] \
                                 for (i,c) in enumerate(self.columns)]
        self.ramp = ramp

    def Start(self):
        self.values = self.mce.read(self.card, self.param)
        if self.start != None:
            for (i,c) in enumerate(self.columns):
                self.values[c] = self.start[i]
            self.mce.write(self.card, self.param, self.values)
            
        if self.ramp != None:
            self.ramp.Reset()

    def Servo(self):
        if self.ramp == None:
            return self.ServoStep()

        while self.ServoStep(update_ramp=True):
            print(self.ramp.i)

    def ServoStep(self, update_ramp=True, write_values=True, read_data=True):
        done = True
        if self.ramp != None:
            if update_ramp:
                done = self.ramp.WriteNext()
            else:
                done = self.ramp.Isdone()

        if read_data:
            frame = self.mce.read_frame(data_only=True, channel_set=self.channel_set)
            d = [ frame[d] - t for (d,t) in zip(self.data_offsets,self.targets) ]
            for (c,d) in zip(self.columns, d):
                self.values[c] += int(self.gain*d)

        print(self.values)
        if write_values:
            self.mce.write(self.card, self.param, self.values)
    
        return done
    

m = mce()
r = Ramp(m, 'sq2', 'fb', param_offset=8, param_count=8,
         start = 0, count = 400, step = 160)
s = ColumnServo(m, MCEStage.SA_fb, columns = list(range(8,16)),
                ramp = r)

s.Start()
s.Servo()
