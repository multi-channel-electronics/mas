sub-dirs:
	cd driver; make
	make -C interfaces
	make -C applications