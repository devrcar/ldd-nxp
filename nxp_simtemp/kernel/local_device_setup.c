#include <linux/module.h>
#include <linux/platform_device.h>

#include "nxp_simtemp.h"

#undef pr_fmt
#define pr_fmt(fmt) "%s : " fmt, __func__

void simtemp_dev_release(struct device *dev);

void simtemp_dev_release(struct device *dev)
{
	pr_info("Simtemp device released \n");
}

/* Create platform data */
simtemp_plat_data_t simtemp_pdata[] = {
	{ .sampling_ms = 1000, .threshold_mC = 25100, .mode = SIMTEMP_MODE_NOISY }
};

/* Create platform device */
struct platform_device platform_simtemp_1 = {
	.name = "simtemp",
	.id = 0,
	.dev = { .platform_data = &simtemp_pdata[0],
		.release = simtemp_dev_release
	}
};

static int __init simtemp_device_platform_init(void)
{
	platform_device_register(&platform_simtemp_1);

	pr_info("Device setup module loaded \n");

	return 0;
}

static void __exit simtemp_device_platform_exit(void)
{
	platform_device_unregister(&platform_simtemp_1);
	pr_info("Device setup module unloaded \n");
}

module_init(simtemp_device_platform_init);
module_exit(simtemp_device_platform_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Module which registers a local platform device");
MODULE_AUTHOR("Roberto Carreon");