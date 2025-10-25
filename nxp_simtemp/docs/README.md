# NXP SimTemp Driver

A Linux kernel module that implements a simulated temperature sensor platform driver. This project demonstrates the implementation of a complete Linux device driver with user-space interfaces.

## Overview

The `nxp_simtemp` driver creates a character device that simulates a temperature sensor with the following features:

- Platform driver integration with Device Tree support
- Character device interface (`/dev/simtemp`)
- Configurable sampling rate and temperature thresholds
- Multiple temperature simulation modes
- Sysfs interface for configuration
- Event notification support through poll/epoll
- Ring buffer implementation for sample storage

## Building

The driver can be built using the provided build script. From the project root run:

```bash
cd scripts
./build.sh
```

Requirements:
- Linux kernel headers
- Build tools (make, gcc)

## Loading the Driver

To simplify loading the driver a helper script is provided (see next section).

1. Load the platform device setup module:
```bash
sudo insmod kernel/local_device_setup.ko
```

2. Load the SimTemp driver:
```bash
sudo insmod kernel/simtemp.ko
```

## Run demo script

To simplify loading the driver and exercising the user CLI, a helper script is provided at `scripts/run_demo.sh`.

What it does:
- Inserts the `simtemp` driver module (and, if needed, the `local_device_setup` module that registers a platform device).
- Waits for the device to appear at `/sys/class/simtemp/simtemp`.
- Configures reasonable defaults via the sysfs attributes (`sampling_ms`, `threshold_mc`, `mode`).
- Runs the CLI test mode (`user/cli/main.py --test`) and reports success/failure.
- Cleans up modules it inserted when finished.

Usage:

```bash
cd scripts
bash ./run_demo.sh
```

Notes:
- The script will `sudo` where necessary to insert/remove kernel modules and to run the CLI (which may need access to `/dev/simtemp`).
- Ensure the modules are built (see "Building" above) so `kernel/simtemp.ko` and `kernel/local_device_setup.ko` exist before running the script.

## Features

### Character Device Interface

- Device: `/dev/simtemp`
- Supports blocking reads returning binary temperature records
- Write operations are not permitted
- Poll/epoll support for event notification:
  - New sample availability
  - Threshold crossing events

### Sysfs Interface

Located under `/sys/class/simtemp/`:

- `sampling_ms` (RW): Sample update period in milliseconds
- `threshold_mC` (RW): Temperature threshold in milli-Celsius
- `mode` (RW): Temperature simulation mode
  - `normal`: Default mode with minimal noise
  - `noisy`: Increased temperature variation
  - `ramp`: Continuous temperature increase
- `stats` (RO): Device statistics

### Temperature Sample Format

```c
struct simtemp_sample {
    __u64 timestamp_ns;  // monotonic timestamp
    __s32 temp_mC;       // milli-degree Celsius (e.g., 44123 = 44.123 °C)
    __u32 flags;         // bit0=NEW_SAMPLE, bit1=THRESHOLD_CROSSED
} __attribute__((packed));
```

## Configuration

### Device Tree

The driver supports the following device tree properties:

```dts
simtemp {
    compatible = "nxp,simtemp";
    nxp,sampling-ms = <500>;     // Default: 500ms
    nxp,threshold_mC = <42000>;  // Default: 42.000 °C
    nxp,mode = "normal";         // Options: normal, noisy, ramp
};
```

### Sysfs Interface

Example usage:

```bash
# Configure sampling rate
echo 100 > /sys/class/simtemp/simtemp/sampling_ms

# Set temperature threshold
echo 45000 > /sys/class/simtemp/simtemp/threshold_mc

# Change simulation mode
echo "noisy" > /sys/class/simtemp/simtemp/mode

# Read statistics
cat /sys/class/simtemp/simtemp/stats
```

## Author

Roberto Carreon

## License

GPL
