obj-m := simtemp.o local_device_setup.o

simtemp-objs := nxp_simtemp.o ring_buff_helper.o nxp_simtemp_sysfs_iface.o nxp_simtemp_dt_helper.o

HOST_KERN_DIR = /lib/modules/$(shell uname -r)/build
# This variable should point to your target kernel source for cross-compiling
TARGET_KERN_DIR = $(HOME)/development/imx93-boot/release/frdm-imx93/tmp/work-shared/imx93frdm/kernel-source
TARGET_ARCH = arm64
TARGET_CROSS_COMPILE = aarch64-linux-gnu-

all:
	make -C $(HOST_KERN_DIR) M=$(PWD) modules
target_cc:
	make ARCH=$(TARGET_ARCH) CROSS_COMPILE=$(TARGET_CROSS_COMPILE) -C $(TARGET_KERN_DIR) M=$(PWD) modules
clean:
	make -C $(HOST_KERN_DIR) M=$(PWD) clean
