import _test

from pymce import MCE

mce = MCE()
print 'Basic read'
data = mce.read_raw(10)
print '  ARZ: ', data[:,5]
print

print 'Frame data object'
data = mce.read_data(10)
print data.data.shape
print

print 'Rectangle extract'
data = mce.read_data(10, extract=True)
print data.data.shape
print

print 'Row-col form'
data = mce.read_data(10, row_col=True)
print data.data.shape
print

print 'Invalid read?'
data = mce.read_data(10, cards=[1,3])
print '    We got None:  ', data==None
