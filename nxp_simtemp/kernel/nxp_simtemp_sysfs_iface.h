#ifndef NXP_SIMTEMP_SYSFS_IFACE_H
#define NXP_SIMTEMP_SYSFS_IFACE_H

#include <linux/device.h>

ssize_t sampling_ms_show(struct device *dev,
				 struct device_attribute *attr, char *buf);

ssize_t sampling_ms_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count);

ssize_t threshold_mc_show(struct device *dev,
				  struct device_attribute *attr, char *buf);

ssize_t threshold_mc_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

ssize_t mode_show(struct device *dev, struct device_attribute *attr,
			  char *buf);

ssize_t mode_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count);

ssize_t stats_show(struct device *dev, struct device_attribute *attr,
			   char *buf);

#endif
