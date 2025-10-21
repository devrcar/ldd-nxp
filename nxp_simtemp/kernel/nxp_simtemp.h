#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/fs.h>

#undef pr_fmt
#define pr_fmt(fmt) "%s : " fmt, __func__

#define TEMP_SAMPLE_BUF_SIZE 4

struct simtemp_sample {
	__u64 timestamp_ns; // monotonic timestamp
	__s32 temp_mC; // milli-degree Celsius (e.g., 44123 = 44.123 °C)
	__u32 flags; // bit0=NEW_SAMPLE, bit1=THRESHOLD_CROSSED
} __attribute__((packed));
typedef struct simtemp_sample simtemp_sample_t;

typedef struct simtemp_ring_buff {
	simtemp_sample_t readings[TEMP_SAMPLE_BUF_SIZE];
	unsigned int head; // next write position
	unsigned int tail; // next read position
} simtemp_ring_buff_t;

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
	simtemp_ring_buff_t *buffer;
	struct delayed_work d_work;
	wait_queue_head_t data_wq;
	dev_t dev_num;
	struct cdev cdev;
	struct class *class_simtemp;
	struct device *device_simtemp;
} simtemp_dev_priv_data_t;

/* File operation function prototypes */
ssize_t simtemp_read(struct file *filp, char __user *buff, size_t count,
		     loff_t *f_pos);
ssize_t simtemp_write(struct file *filp, const char __user *buff, size_t count,
		      loff_t *f_pos);
unsigned int simtemp_poll(struct file *filp, struct poll_table_struct *wait);
int simtemp_open(struct inode *inode, struct file *filp);
int simtemp_release(struct inode *inode, struct file *flip);

int simtemp_platform_driver_probe(struct platform_device *pdev);
void simtemp_platform_driver_remove(struct platform_device *pdev);
