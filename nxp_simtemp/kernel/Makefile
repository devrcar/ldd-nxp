obj-m := simtemp.o local_device_setup.o

simtemp-objs := nxp_simtemp.o ring_buff_helper.o

HOST_KERN_DIR = /lib/modules/$(shell uname -r)/build

all:
	make -C $(HOST_KERN_DIR) M=$(PWD) modules
clean:
	make -C $(HOST_KERN_DIR) M=$(PWD) clean
