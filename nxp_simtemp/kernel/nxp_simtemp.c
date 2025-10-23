#include "nxp_simtemp.h"
#include "ring_buff_helper.h"
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/timekeeping.h>
#include <linux/mutex.h>
#include <linux/poll.h>

/* Default temperature value */
#define DEFAULT_TEMP 25000;

/* Multiple devices support */
#define MAX_DEVICES 10
/* Driver private data structure */
typedef struct simtemp_drv_priv_data {
	unsigned int total_devices;
	dev_t device_num_base;
	struct class *class_simtemp;
} simtemp_drv_priv_data_t;

static simtemp_drv_priv_data_t simtemp_drv_data;

/*
** Function Prototypes
*/
int simtemp_platform_driver_probe(struct platform_device *pdev);
void simtemp_platform_driver_remove(struct platform_device *pdev);
static int32_t simtemp_get_temperature(simtemp_sample_mode_e mode);
static void simtemp_work_handler(struct work_struct *work);

/*************** File operation functions ****************/
ssize_t simtemp_read(struct file *filp, char __user *buff, size_t count,
		     loff_t *f_pos);
ssize_t simtemp_write(struct file *filp, const char __user *buff, size_t count,
		      loff_t *f_pos);
unsigned int simtemp_poll(struct file *filp, struct poll_table_struct *wait);
int simtemp_open(struct inode *inode, struct file *filp);
int simtemp_release(struct inode *inode, struct file *flip);

/*
** File operation structure
*/
static struct file_operations simtemp_fops = { .open = simtemp_open,
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
	int ret;

	simtemp_sample_t sample;

	simtemp_dev_priv_data_t *p_dev_data =
		(simtemp_dev_priv_data_t *)filp->private_data;

	simtemp_ring_buff_t *p_buff = (simtemp_ring_buff_t *)p_dev_data->buffer;

	/* Non blocking call (return if no data is available) */
	if ((filp->f_flags & O_NONBLOCK) && (rb_is_empty(p_buff))) {
		return -EAGAIN;
	}

	/* Blocking call (wait if no data is available) */
	if (wait_event_interruptible(p_dev_data->data_wq,
				     (!rb_is_empty(p_buff))))
		return -ERESTARTSYS;

	if (mutex_lock_interruptible(&p_dev_data->data_mutex))
		return -ERESTARTSYS;

	ret = rb_get(p_buff, &sample);

	mutex_unlock(&p_dev_data->data_mutex);

	if (!ret) {
		/* The ring buffer is empty (not expected) */
		return -EFAULT;
	}

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

	/* Normal read data event */
	if (!rb_is_empty(p_buff)) {
		mask |= (POLLIN | POLLRDNORM);
	}

	/* Threshold crossed (HIPRIO) */
	simtemp_sample_t sample;
	if (rb_peek(p_buff, &sample) && (sample.flags & SIMTEMP_EVT_THRS)) {
		mask |= POLLPRI;
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
static int32_t simtemp_get_temperature(simtemp_sample_mode_e mode)
{
	static int32_t temperature = DEFAULT_TEMP;
	int32_t n_rand;

	switch (mode) {
	case SIMTEMP_MODE_NORMAL:
		temperature = DEFAULT_TEMP;
		wait_for_random_bytes();
		get_random_bytes(&n_rand, sizeof(n_rand));
		n_rand = n_rand % 100; // +/- 100m degree noise
		temperature += n_rand;
		break;
	case SIMTEMP_MODE_NOISY:
		temperature = DEFAULT_TEMP;
		wait_for_random_bytes();
		get_random_bytes(&n_rand, sizeof(n_rand));
		n_rand = n_rand % 1000; // +/- 1 degree noise
		temperature += n_rand;
		break;
	case SIMTEMP_MODE_RAMP:
		temperature += 1000; // 1 degree step
		temperature = temperature % 100000; // 100 degree limit
		break;

	default:
		break;
	}

	return temperature;
}

static void simtemp_work_handler(struct work_struct *work)
{
	/* Get delayed_work struct from callback parameter */
	struct delayed_work *d_work = to_delayed_work(work);

	/* Get device's private data structure (for multiple dev) */
	simtemp_dev_priv_data_t *p_dev_data;
	p_dev_data = container_of(d_work, simtemp_dev_priv_data_t, d_work);

	/* Locking control data */
	mutex_lock(&p_dev_data->config_mutex);

	int p_sampling_ms = p_dev_data->pdata.sampling_ms;
	ktime_t kt = ktime_get_real();
	int32_t new_temp = simtemp_get_temperature(p_dev_data->pdata.mode);
	uint32_t new_flags = SIMTEMP_EVT_NEW;
	new_flags |= (new_temp > p_dev_data->pdata.threshold_mC) ?
			     SIMTEMP_EVT_THRS :
			     0;

	mutex_unlock(&p_dev_data->config_mutex);

	/* Filling sample data */
	simtemp_sample_t sample;
	sample.timestamp_ns = kt;
	sample.temp_mC = new_temp;
	sample.flags = new_flags;

	/* Locking consumer data */
	pr_info("Save triggered: timestamp_ns=%llu, temp_mC=%d\n",
		sample.timestamp_ns, sample.temp_mC);
	mutex_lock(&p_dev_data->data_mutex);
	rb_put(p_dev_data->buffer, &sample);
	mutex_unlock(&p_dev_data->data_mutex);

	wake_up_interruptible(&p_dev_data->data_wq);

	/* For periodic callback */
	schedule_delayed_work(&p_dev_data->d_work,
			      msecs_to_jiffies(p_sampling_ms));
}

/* Called when matched platform device is found */
int simtemp_platform_driver_probe(struct platform_device *pdev)
{
	int ret;

	simtemp_plat_data_t *pdata;
	/* Device private data ptr */
	simtemp_dev_priv_data_t *dev_data;

	pr_info("A device is detected\n");

	/* Dynamically allocate memory for the device private data */
	dev_data = devm_kzalloc(&pdev->dev, sizeof(*dev_data), GFP_KERNEL);
	if (!dev_data) {
		pr_err("Cannot allocate memory\n");
		return -ENOMEM;
	}

	/* Get the platform data */
	pdata = (simtemp_plat_data_t *)dev_get_platdata(&pdev->dev);
	if (!pdata) {
		pr_err("No platform data available\n");
		return -EINVAL;
	}

	/* Save the platform data into the device data structure */
	dev_data->pdata.sampling_ms = pdata->sampling_ms;
	dev_data->pdata.threshold_mC = pdata->threshold_mC;
	dev_data->pdata.mode = pdata->mode;

	pr_info("Device sampling_ms = %d\n", dev_data->pdata.sampling_ms);
	pr_info("Device threshold_mC = %d\n", dev_data->pdata.threshold_mC);
	pr_info("Device mode = %d\n", dev_data->pdata.mode);

	/* Save the device data in the platform device structure */
	dev_set_drvdata(&pdev->dev, dev_data);

	/* Dynamically allocate memory using for the buffer */
	dev_data->buffer =
		devm_kzalloc(&pdev->dev, sizeof(*dev_data->buffer), GFP_KERNEL);
	if (!dev_data->buffer) {
		pr_err("Cannot allocate memory\n");
		return -ENOMEM;
	}

	/* Initialize the data and config mutex */
	mutex_init(&dev_data->data_mutex);
	mutex_init(&dev_data->config_mutex);

	/* Initialize wait_queue */
	init_waitqueue_head(&dev_data->data_wq);

	/* Saving driver global data into specific device data */
	dev_data->class_simtemp = simtemp_drv_data.class_simtemp;
	dev_data->dev_num = simtemp_drv_data.device_num_base + pdev->id;

	/* Do cdev init and cdev add */
	cdev_init(&dev_data->cdev, &simtemp_fops);

	dev_data->cdev.owner = THIS_MODULE;
	ret = cdev_add(&dev_data->cdev, dev_data->dev_num, 1);
	if (ret < 0) {
		pr_err("Cdev add failed\n");
		return ret;
	}

	/* Create device file for the detected platform device */
	dev_data->device_simtemp = device_create(
		dev_data->class_simtemp, NULL, dev_data->dev_num, NULL,
		(pdev->id == 0) ? "simtemp" : "simtemp-%d", pdev->id);
	if (IS_ERR(dev_data->device_simtemp)) {
		pr_err("Device create failed\n");
		ret = PTR_ERR(dev_data->device_simtemp);
		cdev_del(&dev_data->cdev);
		return ret;
	}

	/* Initialize the work queue for periodic callback */
	INIT_DELAYED_WORK(&dev_data->d_work, simtemp_work_handler);
	if (!schedule_delayed_work(
		    &dev_data->d_work,
		    msecs_to_jiffies(dev_data->pdata.sampling_ms))) {
		pr_err("Schedule delayed work failed\n");
		device_destroy(dev_data->class_simtemp, dev_data->dev_num);
		cdev_del(&dev_data->cdev);
		return -EPERM;
	}

	simtemp_drv_data.total_devices++;

	pr_info("Probe was successful\n");

	return 0;
}

/* Called when the device is removed from the system */
void simtemp_platform_driver_remove(struct platform_device *pdev)
{
	simtemp_dev_priv_data_t *dev_data = dev_get_drvdata(&pdev->dev);

	/* Remove periodic callback */
	cancel_delayed_work_sync(&dev_data->d_work);

	/* Remove a device that was created with device_create() */
	device_destroy(dev_data->class_simtemp, dev_data->dev_num);

	/* Remove a cdev entry from the system*/
	cdev_del(&dev_data->cdev);

	simtemp_drv_data.total_devices--;

	pr_info("A device is removed\n");
}

static struct platform_driver simtemp_platform_driver = {
	.probe = simtemp_platform_driver_probe,
	.remove = simtemp_platform_driver_remove,
	.driver = { .name = "simtemp" }
};

/* Module's init entry point */
static int __init simtemp_platform_driver_init(void)
{
	int ret;

	/* Dynamically allocate a device number */
	ret = alloc_chrdev_region(&simtemp_drv_data.device_num_base, 0,
				  MAX_DEVICES, "simtemp");
	if (ret < 0) {
		pr_err("Alloc chrdev failed\n");
		return ret;
	}

	/* Create device class under /sys/class */
	simtemp_drv_data.class_simtemp = class_create("simtemp");
	if (IS_ERR(simtemp_drv_data.class_simtemp)) {
		pr_err("Class creation failed\n");
		ret = PTR_ERR(simtemp_drv_data.class_simtemp);
		unregister_chrdev_region(simtemp_drv_data.device_num_base,
					 MAX_DEVICES);
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
	class_destroy(simtemp_drv_data.class_simtemp);

	/* Unregister device numbers */
	unregister_chrdev_region(simtemp_drv_data.device_num_base, MAX_DEVICES);

	pr_info("simtemp platform driver unloaded\n");
}

module_init(simtemp_platform_driver_init);
module_exit(simtemp_platform_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Roberto Carreon");
MODULE_DESCRIPTION("A simulated temperature sensor kernel module");
