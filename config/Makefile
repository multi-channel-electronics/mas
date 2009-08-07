
TARGETS=$(shell ./makefile.py --targets)
DEPS=$(shell ./makefile.py --dependencies)

$(TARGETS) : $(DEPS)
	./makefile.py

clean:
	rm $(TARGETS)

