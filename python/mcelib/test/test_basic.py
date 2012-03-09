import sys
sys.path.append('../build/lib.linux-i686-2.6')

from pymce import BasicMCE

mce = BasicMCE()

card_args = [
    [1,2,3,4],
    "rc2",
    "RCS",
    2]

print "These should work:"
for c in card_args:
    print "  Decoding %s to:" % repr(c)
    print "    ", mce.card_list(c)

print "These should not work:"
for c in [[3,1], 'sdfa']:
    print "  Decoding %s to:" % repr(c)
    print "    ", mce.card_list(c)


data = mce.read_raw(10)
print data[:,5]
