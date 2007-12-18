# Output module basename
DRIVER_NAME ?= mce_dsp

# Special directories - must be ?= or they get overwritten
PWD ?= $(shell pwd)/
FAKEMCE_DIR ?= fake_mce/
RTAI_DIR   ?= /usr/src/rtai

# Use Makefile.local to set up default configuration options.

include $(PWD)/Makefile.local

# Module configuration options, set to 0 or 1.

FAKEMCE    ?= 0
REALTIME   ?= 0
BIGPHYS    ?= 0


# Compilation options - don't trash CFLAGS, everything breaks.

DEFS = 
INCLUDES = -I$(PWD) -I$(PWD)/../include
LIBS = 
MY_CFLAGS = -Wall

ifeq ($(FAKEMCE),1)

  DEFS += -DFAKEMCE
  INCLUDES += -I$(PWD)/$(FAKEMCE_DIR)

endif #FAKEMCE=1


ifeq ($(REALTIME),1)

  DEFS += -DREALTIME
  INCLUDES += -I$(RTAI_DIR)/include -I$(RTAI_DIR)

endif #REALTIME=1


ifeq ($(BIGPHYS),1)

  DEFS += -DBIGPHYS

endif #BIGPHYS=1



# Set up source files

SOURCE_OBJ := mce_driver.o mce_ops.o \
	dsp_driver.o dsp_ops.o \
	data.o data_ops.o data_watcher.o \
	proc.o

ifeq ($(FAKEMCE),1)

  SOURCE_OBJ += $(FAKEMCE_DIR)dsp_fake.o \
	$(FAKEMCE_DIR)mce_fake.o \
	$(FAKEMCE_DIR)dsp_state.o

else

  SOURCE_OBJ += dsp_pci.o

endif #FAKEMCE=1


# Get kernel version -> KVER

include $(PWD)/Makefile.kversion


#2.4 Kernel

ifeq ($(KVER),2.4)

include $(PWD)/Makefile.2.4

endif # KVER==2.4


#2.6 Kernel

ifeq ($(KVER),2.6)

include $(PWD)/Makefile.2.6

endif


# General rules

$(PWD)/Makefile.local:
	touch $(PWD)/Makefile.local

fault:
	@echo Unknown kernel version $(TARGET_KERNEL) : $(KVER)

clean: kversion_clean
	-rm -rf *.o *~ core .depend .*.cmd *.mod.c .tmp_versions Modules.symvers
ifeq ($(FAKEMCE),1)
	make -C $(FAKEMCE_DIR) clean
endif #FAKEMCE=1


# Install rule

install: modules
	install -d $(INSTALLDIR)
	install $(DRIVER_FILE) $(INSTALLDIR)
	depmod -v $(KERNELVER)