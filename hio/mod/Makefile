KERN_PATH?=/lib/modules/`uname -r`/build/

BUILD_DIR=$(PWD)
PETLIB_PATH=$(BUILD_DIR)/../../petlib

HOBBESDIR 		= $(PWD)/../..
LIBHOBBES 		= $(HOBBESDIR)/libhobbes
PETLIB			= $(HOBBESDIR)/petlib


#XPMEM_PATH=$(BUILD_DIR)/../../xpmem/
#KBUILD_EXTRA_SYMBOLS+=$(XPMEM_PATH)/mod/Module.symvers

EXTRA_CFLAGS += -I$(BUILD_DIR)/include/
#EXTRA_CFLAGS += -I$(XPMEM_PATH)/include/

obj-m += hio.o

hio-objs :=  src/hio_main.o             \
    	     src/hio_stub.o		\
    	     src/hio_engine.o

# All default targets must be invoked via a call to make 
all: 
	make -C $(KERN_PATH) M=$(PWD) modules
	make -C linux_usr/ PETLIB_PATH=$(PETLIB_PATH) LIBHOBBES=$(LIBHOBBES)

clean: 
	make -C $(KERN_PATH) M=$(PWD) clean
	make -C linux_usr/ clean
	rm -f $(shell find src/ -name "*.o")

.PHONY: tags
tags:
	ctags -R src/
