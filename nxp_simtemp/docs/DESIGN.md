# Design

## Block Diagram

flowchart LR
  subgraph KERNEL["Kernel side"]
    direction TB

    PROD["Producer (workqueue)
- Generate simulated temperature sample
- Uses config mutex for control props
- Push to ring buffer (data mutex)
- Wake consumers via wait queue"]:::box

    SYSC["Syscalls
- read/poll used by consumers
- Fetch from ring buffer
- Sleep based on O_NONBLOCK & data readiness"]:::box

    SYSFS["sysfs
- Control interface for user space
- Protected by config mutex
- Attached to class device (child of platform device)"]:::box

    DT["Device Tree (DT)
- Access platform data via binding
- Provide match table; on match, read node properties"]:::box
  end

  subgraph USER["User space"]
    direction TB
    CONS["Consumer (C++/Python)
- Read char device (/dev/simtemp)
- Poll + read (or just read)
- Poll waits for high-priority events (e.g., abnormal temp)"]:::box

    CONF["Configurer
- Control driver via sysfs
- Use standard tools: echo, cat"]:::box
  end

  %% Data/control paths
  PROD -->|"samples"| SYSC
  SYSC -->|"wakeups via waitqueue"| CONS
  CONS -->|"read/poll on /dev/simtemp"| SYSC
  CONF -->|"echo/cat sysfs attrs"| SYSFS

  DT -->|"platform data (OF match, props)"| KERNEL
  SYSFS -->|"updates config"| PROD

  %% Styling
  classDef box fill:#0b74de11,stroke:#0b74de,stroke-width:1px,color:#0b1f33;
  classDef group fill:#cccccc22,stroke:#888,stroke-width:1px,color:#222;
  class KERNEL,USER group;

## Description

### Kernel Side

- Producer (workqueue):
  - Generates a simulated temperature sample based on the control properties (protected by a config mutex).
  - Pushes it to the ring buffer (protected by a data mutex).
  - Wakes up any sleeping thread (consumers via wait queue).

- Syscalls:
  - `read` and `poll` are called by the consumers.
  - Fetch data from the ring buffer.
  - Sleep depending on the non-blocking flag and data readiness.

- sysfs:
  - Provides the control interface for user-space programs.
  - Protected by the config mutex.
  - Attached to the class device (child of the platform device).

- Device Tree (DT):
  - Provides access to the platform data via a device tree binding.
  - The kernel handles most of the device tree processing; provide a match table, and if a match is triggered, obtain the properties of that node.

### User Space

- Consumer:
  - A program (C++/Python) consumes data by reading the character device (data path).
  - This can be done using polling and reading (or just reading).
  - The polling interface allows waiting for high-priority events (such as abnormal temperature).

- Configurer:
  - Uses sysfs to control the driver behavior, with standard commands such as `echo` and `cat`.

## Synchronization and Context

- A mutex is used instead of spinlocks because the consumer can block (sleep), requiring a process context for synchronization.
- The choice of a workqueue follows the same premise; a timer runs in atomic context, whereas the workqueue provides process context suitable for sleeping operations.
