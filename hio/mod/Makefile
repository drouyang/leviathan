KERN_PATH?=/lib/modules/`uname -r`/build/

BUILD_DIR=$(PWD)

EXTRA_CFLAGS += -I$(BUILD_DIR)/include/

obj-m += hio.o

hio-objs :=  src/hio_main.o             \
    	     src/hio_stub.o

# All default targets must be invoked via a call to make 
all: 
	make -C $(KERN_PATH) M=$(PWD) modules

clean: 
	make -C $(KERN_PATH) M=$(PWD) clean
	rm -f $(shell find src/ -name "*.o")

.PHONY: tags
tags:
	ctags -R src/
