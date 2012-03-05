import mcelib
m = mcelib.connect()
print m
print
print 'write...'
mcelib.write(m, "cc", "led", 0, [7, 8, 7])
print 'read...'
print mcelib.read(m, "cc", "fw_rev", 0, -1)
