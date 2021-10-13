from __future__ import division
from __future__ import print_function
from future import standard_library
standard_library.install_aliases()
from builtins import zip
from builtins import range
from past.utils import old_div
from mce import *
from mce_runfile import *
from glob import glob
import sys, subprocess
from numpy import *

def expt_param(key, dtype=None):
    src = '/data/cryo/current_data/experiment.cfg'
    line = subprocess.getoutput('mas_param -s %s get %s' % (src, key))
    s = line.split(' ')
    if dtype == None or dtype == 'string':
        return s
    if dtype == 'int':
        return [ int(ss) for ss in s if ss != '']
    if dtype == 'float':
        return [ float(ss) for ss in s if ss != '']
    raise ValueError('can\'t handle dtype=\'%s\' '%dtype)


def reservo(m, param, gains=None, rows=None, steps=10):
    done = False
    if rows == None:
        rows = [0]*32
    if gains == None:
        gains = [0.02]*32
    while not done:
        data = m.read_frame(data_only=True)
        dy = [data[32*r + c] for (c,r) in enumerate(rows)]
        dx = [g*d for d,g in zip(dy, gains)]
        x = m.read(param[0], param[1])
        x_new = [int(a+b) for (a,b) in zip(x,dx)]
        print(x_new)
        m.write(param[0], param[1], x_new)
        done = True

def set_adcoffset(m, ofs):
    for rc in range(4):
        for c in range(8):
            m.write('rc%i'%(rc+1), 'adc_offset%i'%c,
                      [ofs[rc*8+c]]*41)

def get_historical_offset(folder, stage='ssa', rows=None):
    offsets = []
    if rows == None:
        rows = [0]*32
    for rc in range(4):
        file = glob(folder+'/*RC%i_%s.run'%(rc+1,stage))
        rf = MCERunfile(file[0])
	all_offsets = rf.Item2d('HEADER', 'RB rc%i adc_offset%%i'%(rc+1),
                              	type='int')
        offsets.extend([all_offsets[c][r] for (c,r) in
                        enumerate(rows[rc*8:rc*8+8])])
    return offsets

def write_adc_offset(m, ofs, fill=True, n_rows=33):
    for c in range(32):
        m.write('rc%i'%((old_div(c,8))+1), 'adc_offset%i'%(c%8), [ofs[c]]*41)

    
def get_line(m, rows=None):
    if rows == None:
        rows = [0]*32
    d = m.read_frame(data_only=True)
    return [d[r*32+c] for (c,r) in enumerate(rows)]

def four_to_32(four):
    thirtytwo = []
    for one in four:
        thirtytwo.extend([one]*8)
    return thirtytwo

if __name__ == '__main__':
    m = mce()
    m.write('rca', 'data_mode', [0])

    usage = \
          """
          Usage:
              %s stage tuning
                            "stage" is one of sq1, sq2, sa
                            "tuning" is the full path to the reference tuning
          """ % sys.argv[0]

    if len(sys.argv)!=3:
        print(usage)
        sys.exit(1)
        
    stage = sys.argv[1]
    tuning = sys.argv[2]

    # Regardless of the stage, we can use the ADC_offset from sq2servo:
    ofs = get_historical_offset(tuning, 'sq2servo')
    write_adc_offset(m, ofs)



    if stage == 'sq1':
        # This has no analog in the tuning... sq1_fb hardware servo'd
        param = ['sq1', 'fb_const']
        g = expt_param('default_servo_i', dtype='float')
        gains = [gg/4096. for gg in g]        
        rows = expt_param('sq2_rows', dtype='int')
    elif stage == 'sq2':
        # This is like sq1servo, but the sq1 are off
        param = ['sq2', 'fb']
        g = expt_param('sq1servo_gain', dtype='float')
        gains = four_to_32(g)        
        rows = None
    elif stage == 'sa':
        # This is like sq2servo, but the sq2 are off
        param = ['sa', 'fb']
        g = expt_param('sq2servo_gain', dtype='float')
        gains = four_to_32(g)        
        rows = None

    n_check = 10
    n_servo = 100

    err = [ get_line(m, rows) for i in range(n_check)]
    err = array(err)
    print('Initial error (%i): %10.2f' % (n_check, mean(abs(mean(err,axis=0)))))

    print('Servoing \'%s %s\' for %i steps...' % (param[0],param[1],n_servo))
    for i in range(n_servo):
        reservo(m, param, gains=gains)
    err = array([ get_line(m, rows) for i in range(n_check)])

    print('Final error (%i):   %10.2f' % (n_check, mean(abs(mean(err,axis=0)))))
