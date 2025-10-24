#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <iomanip>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#define DEVICE_FILE "/dev/simtemp"

#define POLL_TEST

#define SIMTEMP_EVT_NEW 0x0001
#define SIMTEMP_EVT_THRS 0x0002

struct simtemp_sample {
	__u64 timestamp_ns; // monotonic timestamp
	__s32 temp_mC; // milli-degree Celsius (e.g., 44123 = 44.123 °C)
	__u32 flags; // bit0=NEW_SAMPLE, bit1=THRESHOLD_CROSSED
} __attribute__((packed));
typedef struct simtemp_sample simtemp_sample_t;


std::string convert_ns_to_ts(__u64 timestamp_ns) {
    /* Create a duration (nanosegundos since Epoch) */
    std::chrono::nanoseconds duration_since_epoch(timestamp_ns);

    /* std::chrono::system_clock::time_point = Epoch + duration */
    auto time_point = std::chrono::system_clock::time_point(duration_since_epoch);

    /* Get fractional part (miliseconds) */
    auto ms_frac = std::chrono::duration_cast<std::chrono::milliseconds>(
                        time_point.time_since_epoch()
                    ).count() % 1000;

    /* UTC format convertion */
    auto timer = std::chrono::system_clock::to_time_t(time_point);
    std::tm tm_buf;
    gmtime_r(&timer, &tm_buf);

    /* Format output string (ISO 8601) */
    std::stringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms_frac << "Z";

    return ss.str();
}

int process_read(int fd, bool is_blocking, simtemp_sample_t *sample) {
    ssize_t bytes_read;
    bytes_read = read(fd, sample, sizeof(*sample));

    if (bytes_read < 0) {
        if (!is_blocking && errno == EAGAIN) {
            std::cout << "\nNon-blocking op: No data available (continue)\n";
            return EXIT_SUCCESS;
        } else {
            std::cerr << "Device read failed\n";
            return EXIT_FAILURE;
        }
    } else {

        if (bytes_read != sizeof(simtemp_sample_t)) {
            std::cerr << "Mismatch error: " << bytes_read << " bytes readed, " << sizeof(simtemp_sample_t) << " bytes expected.\n";
            return EXIT_FAILURE;
        }

        bool alert = (sample->flags & SIMTEMP_EVT_THRS) != 0;
        std::string time_str = convert_ns_to_ts(sample->timestamp_ns);
        std::cout << "[" << time_str << "] Temperature: " << (sample->temp_mC/1000.0) << "C, alert: " << alert << "\n";
    }
    return EXIT_SUCCESS;
}

int main() {
    #ifdef POLL_TEST
    struct pollfd pfd;
    constexpr static int POLL_TIMEOUT = 5000;
    #endif
    int ret;
    int fd;
    simtemp_sample_t temp_sample;
    ssize_t bytes_read;
    bool is_blocking = false;
    int open_flags = O_RDONLY;

    if (!is_blocking) {
        open_flags |= O_NONBLOCK;
    }

    fd = open(DEVICE_FILE, open_flags);
    if (fd < 0) {
        std::cerr << "Device " << DEVICE_FILE << " open failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "Device " << DEVICE_FILE << " open successfully\n";

    #ifdef POLL_TEST
    pfd.fd = fd;
    pfd.events = ( POLLIN | POLLRDNORM | POLLPRI );
    #endif

    while(true) {
        #ifdef POLL_TEST
        std::cout << "\nStarting poll (" << POLL_TIMEOUT << " msec timeout)...\n";
        ret = poll(&pfd,1,POLL_TIMEOUT);
        if (ret < 0) {
            std::cerr << "Poll failed\n";
            return EXIT_FAILURE;
        }
        if (ret == 0) {
            // std::cerr << "Poll timeout\n";
            continue;
        }
        if(pfd.revents & POLLPRI) {
            std::cout << "URGENT: Threshold crossing detected!\n";
        }
        if(pfd.revents & (POLLIN | POLLRDNORM)) {
            std::cout << "Data is ready. Attempting to read..\n";
            ret = process_read(fd, is_blocking, &temp_sample);
            if (ret) {
                close(fd);
                return EXIT_FAILURE;
            }
        }
        #else
        ret = process_read(fd, is_blocking, &temp_sample);
        if (ret) {
            close(fd);
            return EXIT_FAILURE;
        }
        if (!is_blocking) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        #endif
    }

    close(fd);

    return EXIT_SUCCESS;
}