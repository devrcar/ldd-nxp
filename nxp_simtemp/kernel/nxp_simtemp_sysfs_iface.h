#ifndef NXP_SIMTEMP_SYSFS_IFACE_H
#define NXP_SIMTEMP_SYSFS_IFACE_H

#include <linux/device.h>

ssize_t simtemp_sampling_ms_show(struct device *dev,
				 struct device_attribute *attr, char *buf);

ssize_t simtemp_sampling_ms_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count);

ssize_t simtemp_threshold_mc_show(struct device *dev,
				  struct device_attribute *attr, char *buf);

ssize_t simtemp_threshold_mc_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

ssize_t simtemp_mode_show(struct device *dev, struct device_attribute *attr,
			  char *buf);

ssize_t simtemp_mode_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count);

ssize_t simtemp_stats_show(struct device *dev, struct device_attribute *attr,
			   char *buf);

#endif
