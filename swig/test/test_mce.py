import sys
sys.path.append(sys.path[0] + '/..')

from mce import *

m = mce()

print 'This will fail:'
try:
    m.read("sa", "biasf")
except ParamError, e:
    print e

print 'This will not:'
try:
    sa_bias = m.read("sa", "bias")
except:
    print "...that really should have worked."
else:
    print 'sa_bias = ', sa_bias

for i in range(10):
    m.write('sa', 'fb', [i for j in range(32)])
    a = m.read_frame(data_only=True)
    print i, a[0]

print 'done'
