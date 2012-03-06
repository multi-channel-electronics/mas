

CFLAGS += -Wall -fPIC -fno-stack-protector -fno-strict-aliasing
CFLAGS +=  -I/usr/include/python2.6 -I/usr/mce/include

LDFLAGS += -shared -fPIC
LDLINK +=  -L/usr/mce/lib -lmcecmd -lmaslog -lmcedsp -lconfig

default:
	python setup.py build

mcelib.so: base.o
	$(LD) $(LDFLAGS) -o $@ $< $(LDLINK)

clean:
	-rm *.o mcelib.so
	-python setup.py clean

