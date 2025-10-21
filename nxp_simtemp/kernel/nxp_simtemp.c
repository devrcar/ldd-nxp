#include "nxp_simtemp.h"
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/timekeeping.h>
#include <linux/mutex.h>
#include <linux/poll.h>

/* Device private data */
static simtemp_dev_priv_data_t dev_data;

/* lock for temperature buffer access */
static DEFINE_MUTEX(read_access);

/* file operations of the driver (for now dummy ones)*/
struct file_operations simtemp_fops = { .open = simtemp_open,
					.release = simtemp_release,
					.read = simtemp_read,
					.poll = simtemp_poll,
					.write = simtemp_write,
					.llseek = noop_llseek,
					.owner = THIS_MODULE };

ssize_t simtemp_read(struct file *filp, char __user *buff, size_t count,
		     loff_t *f_pos)
{
	pr_info("Read requested for %zu bytes \n", count);

	simtemp_sample_t sample;

	simtemp_dev_priv_data_t *p_dev_data =
		(simtemp_dev_priv_data_t *)filp->private_data;

	simtemp_ring_buff_t *p_buff = (simtemp_ring_buff_t *)p_dev_data->buffer;

	/* Non blocking call (return if no data is available) */
	if ((filp->f_flags & O_NONBLOCK) && (p_buff->tail == p_buff->head)) {
		return -EAGAIN;
	}

	/* Blocking call (wait if no data is available) */
	if (wait_event_interruptible(p_dev_data->data_wq,
				     (p_buff->tail != p_buff->head)))
		return -ERESTARTSYS;

	if (mutex_lock_interruptible(&read_access))
		return -ERESTARTSYS;

	sample = p_buff->readings[p_buff->tail];
	p_buff->tail = (p_buff->tail + 1) % TEMP_SAMPLE_BUF_SIZE;
	mutex_unlock(&read_access);

	if (copy_to_user(buff, &sample, sizeof(sample))) {
		return -EFAULT;
	}

	return sizeof(sample);
}

ssize_t simtemp_write(struct file *filp, const char __user *buff, size_t count,
		      loff_t *f_pos)
{
	pr_err("Write operation not permited \n");

	return -EPERM;
}

unsigned int simtemp_poll(struct file *filp, struct poll_table_struct *wait)
{
	pr_info("Poll requested \n");
	unsigned int mask = 0;

	simtemp_dev_priv_data_t *p_dev_data =
		(simtemp_dev_priv_data_t *)filp->private_data;

	simtemp_ring_buff_t *p_buff = (simtemp_ring_buff_t *)p_dev_data->buffer;

	poll_wait(filp, &p_dev_data->data_wq, wait);

	if (p_buff->tail != p_buff->head) {
		mask |= (POLLIN | POLLRDNORM);
	}

	return mask;
}

int simtemp_open(struct inode *inode, struct file *filp)
{
	pr_info("minor access = %d\n", MINOR(inode->i_rdev));
	simtemp_dev_priv_data_t *p_dev_data;

	/* Get device's private data structure (for multiple dev) */
	p_dev_data = container_of(inode->i_cdev, simtemp_dev_priv_data_t, cdev);

	/* To supply device private data to FOPS methods of the driver */
	filp->private_data = p_dev_data;

	/* The char device is only for readings (data path) */
	if ((filp->f_mode & FMODE_WRITE)) {
		pr_warn("Open was unsuccessful (Write op not permited)\n");
		return -EPERM;
	}

	pr_info("Open was successful\n");

	return 0;
}

int simtemp_release(struct inode *inode, struct file *flip)
{
	pr_info("release was successful\n");

	return 0;
}

/* Work handler code */
static int32_t simtemp_get_temperature(void)
{
	/* Default temperature vaule */
	int32_t temperature = 25000;

	int32_t n_rand;
	wait_for_random_bytes();
	get_random_bytes(&n_rand, sizeof(n_rand));
	n_rand = n_rand % 1000;

	temperature += n_rand;

	return temperature;
}

static void save_reading(simtemp_ring_buff_t *rb, simtemp_sample_t *sample)
{
	pr_info("Save triggered: timestamp_ns=%llu, temp_mC=%d\n",
		sample->timestamp_ns, sample->temp_mC);

	mutex_lock(&read_access);

	rb->readings[rb->head] = *sample;
	rb->head = (rb->head + 1) % TEMP_SAMPLE_BUF_SIZE;

	/* Move tail if buffer is full */
	if (rb->head == rb->tail) {
		rb->tail = (rb->tail + 1) % TEMP_SAMPLE_BUF_SIZE;
	}
	mutex_unlock(&read_access);
}

static void simtemp_work_handler(struct work_struct *work)
{
	pr_info("Worker callback triggered\n");
	/* Get delayed_work struct from callback parameter */
	struct delayed_work *d_work = to_delayed_work(work);

	/* Get device's private data structure (for multiple dev) */
	simtemp_dev_priv_data_t *p_dev_data;
	p_dev_data = container_of(d_work, simtemp_dev_priv_data_t, d_work);

	simtemp_sample_t sample;
	ktime_t kt = ktime_get_real();

	sample.timestamp_ns = kt;
	sample.temp_mC = simtemp_get_temperature();
	sample.flags = 0;

	save_reading(p_dev_data->buffer, &sample);

	wake_up_interruptible(&p_dev_data->data_wq);

	/* For periodic callback */
	schedule_delayed_work(&p_dev_data->d_work,
			      msecs_to_jiffies(dev_data.pdata.sampling_ms));
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

	/* Dynamically allocate memory for the device buffer */
	dev_data.buffer =
		devm_kzalloc(&pdev->dev, sizeof(*dev_data.buffer), GFP_KERNEL);
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

	INIT_DELAYED_WORK(&dev_data.d_work, simtemp_work_handler);
	if (!schedule_delayed_work(
		    &dev_data.d_work,
		    msecs_to_jiffies(dev_data.pdata.sampling_ms))) {
		pr_err("Schedule delayed work failed\n");
		cdev_del(&dev_data.cdev);
		return -EPERM;
	}

	init_waitqueue_head(&dev_data.data_wq);

	pr_info("Probe was successful\n");

	return 0;
}

/* Called when the device is removed from the system */
void simtemp_platform_driver_remove(struct platform_device *pdev)
{
	/* Remove periodic callback */
	cancel_delayed_work_sync(&dev_data.d_work);

	/* Remove a device that was created with device_create() */
	device_destroy(dev_data.class_simtemp, dev_data.dev_num);

	/* Remove a cdev entry from the system*/
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
