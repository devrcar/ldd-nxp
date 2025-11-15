/* compat.h */
#ifndef SIMTEMP_COMPAT_H
#define SIMTEMP_COMPAT_H

#include "nxp_simtemp.h"
#include <linux/version.h>
#include <linux/platform_device.h>

/* Sanity: ensure linux/version.h is available from kernel headers.
 * LINUX_VERSION_CODE comes from the headers you build against. */
#ifndef LINUX_VERSION_CODE
# error "linux/version.h missing: ensure you build against prepared kernel headers"
#endif

/* Detect kernels where platform_driver::remove is void.
 * Core change landed around v6.11 after a transition with remove_new(void). */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,11,0)
#define SIMTEMP_HAVE_PLATFORM_REMOVE_VOID 1
#endif

/* Adapter to match the platform_driver signature on each kernel. */
#if defined(SIMTEMP_HAVE_PLATFORM_REMOVE_VOID)
/* ≥ 6.11: void remove(struct platform_device *pdev) */
static inline void simtemp_platform_remove(struct platform_device *pdev)
{
	simtemp_platform_driver_remove_impl(pdev);
}
#define SIMTEMP_PLATFORM_REMOVE simtemp_platform_remove
#else
/* ≤ 6.10: int remove(struct platform_device *pdev) */
static inline int simtemp_platform_remove_compat(struct platform_device *pdev)
{
	simtemp_platform_driver_remove_impl(pdev);
	return 0; /* return value is ignored by driver core */
}
#define SIMTEMP_PLATFORM_REMOVE simtemp_platform_remove_compat
#endif

#endif /* SIMTEMP_COMPAT_H */

