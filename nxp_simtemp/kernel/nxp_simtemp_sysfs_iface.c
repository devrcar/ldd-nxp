#include "nxp_simtemp_sysfs_iface.h"
#include "nxp_simtemp.h"

ssize_t simtemp_sampling_ms_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ret;
	simtemp_dev_priv_data_t *p_dev_data = dev_get_drvdata(dev);

	mutex_lock(&p_dev_data->config_mutex);

	// Use snprintf to safely format the data into the output buffer
	ret = snprintf(buf, PAGE_SIZE, "%u\n", p_dev_data->pdata.sampling_ms);

	mutex_unlock(&p_dev_data->config_mutex);

	return ret;
}

ssize_t simtemp_sampling_ms_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int ret;
	unsigned int new_period;
	simtemp_dev_priv_data_t *p_dev_data = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 10, &new_period);
	if (ret)
		return ret;

	if (new_period < 1 || new_period > 50000) /* Validate the range */
		return -EINVAL;

	mutex_lock(&p_dev_data->config_mutex);

	/* First cancel the callback */
	cancel_delayed_work_sync(&p_dev_data->d_work);

	/* Update the sampling period */
	p_dev_data->pdata.sampling_ms = new_period;

	/* Reschedule callback */
	schedule_delayed_work(&p_dev_data->d_work,
			      msecs_to_jiffies(new_period));

	mutex_unlock(&p_dev_data->config_mutex);

	return count;
}

ssize_t simtemp_threshold_mc_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int ret;
	simtemp_dev_priv_data_t *p_dev_data = dev_get_drvdata(dev);

	mutex_lock(&p_dev_data->config_mutex);

	// Use snprintf to safely format the data into the output buffer
	ret = snprintf(buf, PAGE_SIZE, "%d\n", p_dev_data->pdata.threshold_mC);

	mutex_unlock(&p_dev_data->config_mutex);

	return ret;
}

ssize_t simtemp_threshold_mc_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	int new_threshold;
	simtemp_dev_priv_data_t *p_dev_data = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 10, &new_threshold);
	if (ret)
		return ret;

	mutex_lock(&p_dev_data->config_mutex);

	/* Update the threshold */
	p_dev_data->pdata.threshold_mC = new_threshold;

	mutex_unlock(&p_dev_data->config_mutex);

	return count;
}

static const char *mode_strings[] = {
	[SIMTEMP_MODE_NORMAL] = "normal",
	[SIMTEMP_MODE_NOISY] = "noisy",
	[SIMTEMP_MODE_RAMP] = "ramp",
};

ssize_t simtemp_mode_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	int ret;
	simtemp_dev_priv_data_t *p_dev_data = dev_get_drvdata(dev);

	mutex_lock(&p_dev_data->config_mutex);

	// Use snprintf to safely format the data into the output buffer
	ret = snprintf(buf, PAGE_SIZE, "%s\n",
		       mode_strings[p_dev_data->pdata.mode]);

	mutex_unlock(&p_dev_data->config_mutex);

	return ret;
}

ssize_t simtemp_mode_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int index;
	bool match = false;
	simtemp_dev_priv_data_t *p_dev_data = dev_get_drvdata(dev);

	/* Compare input string with each mode */
	for (index = 0; index < ARRAY_SIZE(mode_strings); index++) {
		if (sysfs_streq(buf, mode_strings[index])) {
			match = true;
			break;
		}
	}

	if (!match) {
		return -EINVAL;
	}

	mutex_lock(&p_dev_data->config_mutex);

	/* Update the mode */
	p_dev_data->pdata.mode = index;

	mutex_unlock(&p_dev_data->config_mutex);

	return count;
}

ssize_t simtemp_stats_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	int ret;
	simtemp_dev_priv_data_t *p_dev_data = dev_get_drvdata(dev);

	mutex_lock(&p_dev_data->config_mutex);

	// Use snprintf to safely format the data into the output buffer
	ret = snprintf(buf, PAGE_SIZE, "N of updates: %lu, N of alerts: %lu\n",
		       p_dev_data->update_count, p_dev_data->alert_count);

	mutex_unlock(&p_dev_data->config_mutex);

	return ret;
}
