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
- python3 (pip)
- python3-venv (venv module for python3)

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

To simplify loading the driver and exercising the user CLI/GUI, a helper script is provided at `scripts/run_demo.sh`.

What it does:

- Inserts the `simtemp` driver module (and, if needed, the `local_device_setup` module that registers a platform device).
- Waits for the device to appear at `/sys/class/simtemp/simtemp`.
- Configures reasonable defaults via the sysfs attributes (`sampling_ms`, `threshold_mc`, `mode`).
- Launches either the CLI or the GUI depending on the argument (see Usage below).
- Cleans up modules it inserted when finished.

Usage:

```bash
cd scripts
# run the CLI test (default)
./run_demo.sh --cli

# launch the GUI application (will try to use the project's venv)
./run_demo.sh --gui
```

GUI behavior notes:

- The script will look for a Python virtual environment at `user/.venv` and execute the GUI from there so installed Python packages (for example `matplotlib`) are available.

Notes:

- The script will `sudo` where necessary to insert/remove kernel modules and to launch the CLI/GUI (which may need access to `/dev/simtemp` and the sysfs attributes).
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

## Repository

Source code and project resources are available on GitHub:
https://github.com/devrcar/ldd-nxp.git

Driver demo video:
https://drive.google.com/file/d/1bQCeVYDt38xgN79Qd0WfaTGJj0jm5kp7/view?usp=sharing

Driver GUI video:
https://drive.google.com/file/d/1lY8hJbapfbY0C15zp-CQLZPazx8UEUoS/view?usp=sharing