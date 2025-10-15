#include "nxp_simtemp.h"
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#define DEVICE_BUFF_SIZE 1024

/* Device private data */
simtemp_dev_priv_data_t dev_data;

/* file operations of the driver (for now dummy ones)*/
struct file_operations simtemp_fops = { .open = simtemp_open,
					.release = simtemp_release,
					.read = simtemp_read,
					.write = simtemp_write,
					.llseek = simtemp_lseek,
					.owner = THIS_MODULE };

loff_t simtemp_lseek(struct file *filp, loff_t offset, int whence)
{
	pr_info("lseek requested \n");
	pr_info("Current value of the file position = %lld\n", filp->f_pos);

	return filp->f_pos;
}

ssize_t simtemp_read(struct file *filp, char __user *buff, size_t count,
		     loff_t *f_pos)
{
	pr_info("Read requested for %zu bytes \n", count);
	pr_info("Current file position = %lld\n", *f_pos);

	return 0;
}

ssize_t simtemp_write(struct file *filp, const char __user *buff, size_t count,
		      loff_t *f_pos)
{
	pr_info("Write requested for %zu bytes\n", count);
	pr_info("Current file position = %lld\n", *f_pos);

	return 0;
}

int simtemp_open(struct inode *inode, struct file *filp)
{
	pr_info("minor access = %d\n", MINOR(inode->i_rdev));

	return 0;
}

int simtemp_release(struct inode *inode, struct file *flip)
{
	pr_info("release was successful\n");

	return 0;
}

/* Called when matched platform device is found */
int simtemp_platform_driver_probe(struct platform_device *pdev)
{
	int ret;

	simtemp_plat_data_t *pdata;

	pr_info("A device is detected\n");

	/* Get the platform data */
	pdata = (simtemp_plat_data_t *)dev_get_platdata(&pdev->dev);
	if (!pdata) {
		pr_err("No platform data available\n");
		return -EINVAL;
	}

	/* Save the platform data into the device data structure */
	dev_data.pdata.sampling_ms = pdata->sampling_ms;
	dev_data.pdata.threshold_mC = pdata->threshold_mC;
	dev_data.pdata.mode = pdata->mode;

	pr_info("Device sampling_ms = %d\n", dev_data.pdata.sampling_ms);
	pr_info("Device threshold_mC = %d\n", dev_data.pdata.threshold_mC);
	pr_info("Device mode = %d\n", dev_data.pdata.mode);

	/*Dynamically allocate memory for the device buffer */
	dev_data.buffer =
		devm_kzalloc(&pdev->dev, DEVICE_BUFF_SIZE, GFP_KERNEL);
	if (!dev_data.buffer) {
		pr_info("Cannot allocate memory \n");
		return -ENOMEM;
	}

	/* Save the device data in the platform device structure */
	dev_set_drvdata(&pdev->dev, &dev_data);

	/* Do cdev init and cdev add */
	cdev_init(&dev_data.cdev, &simtemp_fops);

	dev_data.cdev.owner = THIS_MODULE;
	ret = cdev_add(&dev_data.cdev, dev_data.dev_num, 1);
	if (ret < 0) {
		pr_err("Cdev add failed\n");
		return ret;
	}

	/*Create device file for the detected platform device */
	dev_data.device_simtemp = device_create(dev_data.class_simtemp, NULL,
						dev_data.dev_num, NULL,
						"simtemp");
	if (IS_ERR(dev_data.device_simtemp)) {
		pr_err("Device create failed\n");
		ret = PTR_ERR(dev_data.device_simtemp);
		cdev_del(&dev_data.cdev);
		return ret;
	}

	pr_info("Probe was successful\n");

	return 0;
}

/* Called when the device is removed from the system */
void simtemp_platform_driver_remove(struct platform_device *pdev)
{
	/*1. Remove a device that was created with device_create() */
	device_destroy(dev_data.class_simtemp, dev_data.dev_num);

	/*2. Remove a cdev entry from the system*/
	cdev_del(&dev_data.cdev);

	pr_info("A device is removed\n");
}

struct platform_driver simtemp_platform_driver = {
	.probe = simtemp_platform_driver_probe,
	.remove = simtemp_platform_driver_remove,
	.driver = { .name = "simtemp" }
};

/* Module's init entry point */
static int __init simtemp_platform_driver_init(void)
{
	int ret;

	/* Dynamically allocate a device number */
	ret = alloc_chrdev_region(&dev_data.dev_num, 0, 1, "simtemp");
	if (ret < 0) {
		pr_err("Alloc chrdev failed\n");
		return ret;
	}

	/* Create device class under /sys/class */
	dev_data.class_simtemp = class_create("simtemp");
	if (IS_ERR(dev_data.class_simtemp)) {
		pr_err("Class creation failed\n");
		ret = PTR_ERR(dev_data.class_simtemp);
		unregister_chrdev_region(dev_data.dev_num, 1);
		return ret;
	}

	/* Register a platform driver */
	platform_driver_register(&simtemp_platform_driver);

	pr_info("simtemp platform driver loaded\n");

	return 0;
}

/* Module's cleanup entry point */
static void __exit simtemp_platform_driver_cleanup(void)
{
	/* Unregister the platform driver */
	platform_driver_unregister(&simtemp_platform_driver);

	/* Class destroy */
	class_destroy(dev_data.class_simtemp);

	/* Unregister device numbers */
	unregister_chrdev_region(dev_data.dev_num, 1);

	pr_info("simtemp platform driver unloaded\n");
}

module_init(simtemp_platform_driver_init);
module_exit(simtemp_platform_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roberto Carreon");
MODULE_DESCRIPTION("A simulated temperature sensor kernel module");
