from mce import *
from mce_runfile import *
from glob import glob

def reservo(mce, param, gain=0.002, rows=None, steps=10):
    done = False
    if rows == None:
        rows = [0]*32
    while not done:
        data = mce.read_frame(data_only=True)
        dy = [data[32*r + c] for (c,r) in enumerate(rows)]
        dx = [gain*d for d in dy]
        x = mce.read(param[0], param[1])
        x_new = [int(a+b) for (a,b) in zip(x,dx)]
        print x_new
        mce.write(param[0], param[1], x_new)
        done = True

def set_adcoffset(ofs):
    
    pass

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

def write_adc_offset(mce, ofs, fill=True, n_rows=33):
    for c in range(32):
        mce.write('rc%i'%((c/8)+1), 'adc_offset%i'%(c%8), [ofs[c]]*8)

    
def get_line(mce, rows=None):
    if rows == None:
        rows = [0]*32
    d = mce.read_frame(data_only=True)
    return [d[r*32+c] for (c,r) in enumerate(rows)]

if __name__ == '__main__':
    mce = mce()
    mce.write('rca', 'data_mode', [0])

    
    stage = 'sq2servo'
    ofs = get_historical_offset('/data/cryo/current_data/1230072210/', stage)
    print ofs
    
    write_adc_offset(mce, ofs)
    x1 = get_line(mce)

    for i in range(100):
        print i
        reservo(mce, ['sq2', 'fb'])
        print get_line(mce)
    
    x2 = get_line(mce)
    
