#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>

#undef pr_fmt
#define pr_fmt(fmt) "%s : " fmt, __func__

/* Mode codes */
typedef enum simtemp_mode_code {
	NORMAL,
	NOISY,
	RAMP,
} simtemp_mode_code_e;

/* Platform data of the simtemp */
typedef struct simtemp_plat_data {
	int sampling_ms;
	int threshold_mC;
	simtemp_mode_code_e mode;
} simtemp_plat_data_t;

/*Device private data structure */
typedef struct simtemp_dev_priv_data {
	simtemp_plat_data_t pdata;
	char *buffer;
    dev_t dev_num;
	struct cdev cdev;
	struct class *class_simtemp;
	struct device *device_simtemp;
} simtemp_dev_priv_data_t;

loff_t simtemp_lseek(struct file *filp, loff_t offset, int whence);
ssize_t simtemp_read(struct file *filp, char __user *buff, size_t count,
		     loff_t *f_pos);
ssize_t simtemp_write(struct file *filp, const char __user *buff, size_t count,
		      loff_t *f_pos);
int simtemp_open(struct inode *inode, struct file *filp);
int simtemp_release(struct inode *inode, struct file *flip);

int simtemp_platform_driver_probe(struct platform_device *pdev);
void simtemp_platform_driver_remove(struct platform_device *pdev);
