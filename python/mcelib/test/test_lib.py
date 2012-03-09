"""
Low-level library connection demonstration.
"""

import _test

from pymce import mcelib

print 'Connecting...'
m = mcelib.connect()
print '  ', m
print
print 'write...'
mcelib.write(m, "cc", "led", 0, [7])
print

print 'read...'
print '   %x' % mcelib.read(m, "cc", "fw_rev", 0, -1)[0]
print

print 'data...'
import numpy
z = numpy.zeros(10000, 'int32')
mcelib.read_data(m, 1, 1, z)
print '  %x' % z[0]
