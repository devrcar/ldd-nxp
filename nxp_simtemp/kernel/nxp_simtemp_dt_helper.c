#include "nxp_simtemp_dt_helper.h"
#include <linux/of.h>
#include <linux/of_device.h>

simtemp_plat_data_t *simtemp_dev_get_platdata_from_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	simtemp_plat_data_t *pdata;
	const char *mode_str;
	int ret;

	if (!np) {
		/* this probe didnt happen because of device tree node */
		return NULL;
	}

	/* Dynamically allocate memory for the device platform data */
	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "Cannot allocate memory\n");
		return ERR_PTR(-ENOMEM);
	}

	if (of_property_read_u32(np, "nxp,sampling-ms", &pdata->sampling_ms)) {
		dev_warn(dev, "Missing sampling-ms property (using default)\n");
		pdata->sampling_ms = SIMTEMP_DEFAULT_SAMPLING_MS;
	}

	if (of_property_read_s32(np, "nxp,threshold-mC",
				 &pdata->threshold_mC)) {
		dev_warn(dev,
			 "Missing threshold_mC property (using default)\n");
		pdata->threshold_mC = SIMTEMP_DEFAULT_THRESHOLD_MC;
	}

	ret = of_property_read_string(np, "nxp,mode", &mode_str);
	if (ret == 0) {
		if (strcmp(mode_str, "normal") == 0)
			pdata->mode = SIMTEMP_MODE_NORMAL;
		else if (strcmp(mode_str, "noisy") == 0)
			pdata->mode = SIMTEMP_MODE_NOISY;
		else if (strcmp(mode_str, "ramp") == 0)
			pdata->mode = SIMTEMP_MODE_RAMP;
		else
			pdata->mode = SIMTEMP_MODE_NORMAL;
	} else {
		dev_warn(dev, "Missing mode property (using default)\n");
		pdata->mode = SIMTEMP_MODE_NORMAL;
	}

	return pdata;
}
