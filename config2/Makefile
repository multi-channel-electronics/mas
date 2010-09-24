default: mce.cfg

BASE := ../
include $(BASE)Makefile.children


SOURCE = mce.cin
TARGET = mce.cfg

# General rule...
%.cfg : %.cin components/
	php $< > $@ || rm -f $@

$(SOURCE) :
	@echo "$(SOURCE) does not exit... can not produce mce.cfg"
	@echo "Create $(SOURCE) from your preferred template." && false

install:
	install --group=$(MAS_GROUP) --mode=775 -d /etc/mce
	install --group=$(MAS_GROUP) --mode=664 mas.cfg /etc/mce/
	install --group=$(MAS_GROUP) --mode=664 $(TARGET) /etc/mce/mce.cfg

clean:
	rm -f $(TARGET)

