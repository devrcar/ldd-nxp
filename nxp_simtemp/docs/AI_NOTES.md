# AI Notes

## Used AI Tools
- Perplexity AI
- Google Gemini

## Validation Process

I validated the corresponding responses based on online resources. For example, when the AI provided kernel code examples, I verified the APIs in the source code and its documentation (https://elixir.bootlin.com/linux/).

I also consulted online examples (https://embetronicx.com/tutorials/linux/).

In addition to this, before starting the challenge, I started a Linux driver development course to gain more context about the development environment (https://www.udemy.com/course/linux-device-driver-programming-using-beaglebone-black/).

---

## Relevant AI Prompts Used (Sorted by Date)

### 1. Initial Driver Development Setup
> I'm going to develop a simple device driver (kernel module) for a simulated temperature sensor. I will register a platform driver, use a device tree binding and provide sysfs control.
> I have limited experience in linux development, so help me to understand the development flow, key concepts, etc.

### 2. Understanding Timing Mechanisms
> My driver requires a periodic call to generate simulated temperature lectures, in this context explain "timer/hrtimer/workqueue", which is more adequate?

### 3. Kernel Ring Buffer Consideration
> Should I use a kernel ring buffer with wait queues for readers

### 4. Locking and Synchronization
> I'm going to develop a simple device driver (kernel module) for a simulated temperature sensor. I will register a platform driver, and provide sysfs for control and a char device for readings. For periodic readings I will use a workqueue (or timer).
> I have limited experience in linux development, so help me to understand the locking and synchronization choices for a producer and multiple consumers.

### 5. Blocking/Non-blocking Reads
> Great, now I want to explore the consumer blocking/non blocking reads, for example how can I support non blocking readings and polling (poll / epoll)

### 6. Custom Poll Events
> Regarding the poll function, kernel and user space can use custom events? I mean custom mask or flags other than the ones defined? My driver has this requirements with poll function: "Wake on new sample availability. Wake when threshold is crossed (separate event bit)."
> But I'm not sure what does that mean

### 7. Sysfs Control Implementation
> Great, the next step in my driver development is to add sysfs control, this will include:
> - `sampling_ms` (RW): update period.
> - `threshold_mC` (RW): alert threshold in milli‑°C.
> - `mode` (RW): e.g., normal|noisy|ramp.
> - `stats` (RO): counters (updates, alerts, last error)
> 
> Explain me how to do it.

### 8. Device Tree Support
> Great, the next step in my driver development is to add device tree support, this will include to get the platform data from device tree.
> Explain me how to do it.

### 9. User Space CLI Application
> Now I need to implement a lightweight user space CLI app on c++ or python, help me to build that.
> The cli app should:
> - Configure sampling period & threshold via sysfs
> - Read from /dev/simtemp using poll and print lines like: `2025-09-22T20:15:04.123Z temp=44.1C alert=0`
