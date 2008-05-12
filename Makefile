default: sub-dirs

sub-dirs:
	cd driver; make
	make -C interfaces
	make -C applications

clean:
	cd driver ; make clean
	make -C interfaces   clean
	make -C applications clean

install: sub-dirs
	cd driver; make install
	make -C include      install
	make -C interfaces   install
	make -C applications install
