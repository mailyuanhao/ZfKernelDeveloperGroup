ifneq ($(KERNELRELEASE),)
	obj-m := globalmem.o
    globalmem-objs := globalmem_main.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions
	rm -rf Module.symvers modules.order
