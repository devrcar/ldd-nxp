#ifndef NXP_SIMTEMP_H
#define NXP_SIMTEMP_H

#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/mutex.h>

#undef pr_fmt
#define pr_fmt(fmt) "%s : " fmt, __func__

#define SIMTEMP_DEFAULT_SAMPLING_MS 500
#define SIMTEMP_DEFAULT_THRESHOLD_MC 42000

#define TEMP_SAMPLE_BUF_SIZE 4

#define SIMTEMP_EVT_NEW 0x0001
#define SIMTEMP_EVT_THRS 0x0002

struct simtemp_sample {
	__u64 timestamp_ns; // monotonic timestamp
	__s32 temp_mC; // milli-degree Celsius (e.g., 44123 = 44.123 °C)
	__u32 flags; // bit0=NEW_SAMPLE, bit1=THRESHOLD_CROSSED
} __attribute__((packed));
typedef struct simtemp_sample simtemp_sample_t;

/* Simulated temperature mode */
typedef enum simtemp_sample_mode {
	SIMTEMP_MODE_NORMAL,
	SIMTEMP_MODE_NOISY,
	SIMTEMP_MODE_RAMP,
} simtemp_sample_mode_e;

/* Ring buffer struct */
typedef struct simtemp_ring_buff {
	simtemp_sample_t readings[TEMP_SAMPLE_BUF_SIZE];
	unsigned int head; // next write position
	unsigned int tail; // next read position
} simtemp_ring_buff_t;

/* Platform data of the simtemp */
typedef struct simtemp_plat_data {
	unsigned int sampling_ms;
	int threshold_mC;
	simtemp_sample_mode_e mode;
} simtemp_plat_data_t;

/* Device private data structure */
typedef struct simtemp_dev_priv_data {
	simtemp_plat_data_t pdata;
	simtemp_ring_buff_t *buffer;

	/* Statistics (read-only) */
	unsigned long update_count;
	unsigned long alert_count;

	struct delayed_work d_work;
	wait_queue_head_t data_wq;
	struct mutex data_mutex;
	struct mutex config_mutex;

	dev_t dev_num;
	struct cdev cdev;
	struct class *class_simtemp;
	struct device *device_simtemp;
} simtemp_dev_priv_data_t;

/* remove platform function for Kernel compatibility */
void simtemp_platform_driver_remove_impl(struct platform_device *pdev);

#endif
