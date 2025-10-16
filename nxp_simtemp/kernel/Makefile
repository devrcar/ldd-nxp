obj-m := nxp_simtemp.o local_device_setup.o

HOST_KERN_DIR = /lib/modules/$(shell uname -r)/build

all:
	make -C $(HOST_KERN_DIR) M=$(PWD) modules
clean:
	make -C $(HOST_KERN_DIR) M=$(PWD) clean
