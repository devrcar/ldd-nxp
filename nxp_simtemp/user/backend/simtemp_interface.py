"""Core temperature sensor interface - shared by CLI and GUI"""
import sys
import os
import struct
import select
from datetime import datetime, timezone
from dataclasses import dataclass
from typing import Optional, Callable

# Size of the data structure read from the character device (8 byte timestamp + 4 byte temp + 4 byte flags = 16 bytes)
SAMPLE_SIZE = 16
# Struct format: < (little-endian), Q (uint64_t timestamp_ns), i (int32_t temp_mC), I (uint32_t flags)
STRUCT_FORMAT = '<QiI'
# Event flags inside the sample data
SIMTEMP_EVT_NEW = 0x0001
SIMTEMP_EVT_THRS = 0x0002

@dataclass
class SensorReading:
    """Temperature reading with metadata"""
    timestamp: str
    temp_c: float
    is_alert: bool
    
    def __str__(self):
        return f"{self.timestamp} temp={self.temp_c:.1f}C alert={int(self.is_alert)}"

def format_timestamp(ts_ns):
    """
    Converts nanoseconds since epoch (ts_ns) to a precise ISO 8601 UTC string 
    with millisecond precision (e.g., 2025-09-22T20:15:04.123Z).
    """
    if ts_ns < 0:
        return "1970-01-01T00:00:00.000Z"
        
    # 1. Convert nanoseconds (integer) to seconds (float)
    # 1 second = 1,000,000,000 nanoseconds
    ts_s = ts_ns / 1_000_000_000.0
    
    # 2. Create datetime object in UTC timezone
    # datetime.fromtimestamp handles the fractional seconds.
    dt_utc = datetime.fromtimestamp(ts_s, tz=timezone.utc)
    
    # 3. Format the string using ISO 8601 format with millisecond precision
    # isoformat(timespec='milliseconds') outputs 'YYYY-MM-DDTHH:MM:SS.mmm+00:00'
    iso_time = dt_utc.isoformat(timespec='milliseconds')
    
    # 4. Replace the "+00:00" UTC offset with the required 'Z' (Zulu time)
    return iso_time.replace('+00:00', 'Z')


class SimTempSensorInterface:
    """Backend interface to temperature sensor driver"""
    
    def __init__(self, sysfs_path="/sys/class/simtemp/simtemp",
                 dev_path="/dev/simtemp"):
        self.sysfs_path = sysfs_path
        self.dev_path = dev_path
        self._fd = None
        self._poller = None
    
    def write_sysfs(self, attr: str, value) -> bool:
        """Write to sysfs attribute"""
        path = os.path.join(self.sysfs_path, attr)
        try:
            with open(path, 'w') as f:
                f.write(str(value))
            return True
        except FileNotFoundError:
            print(f" Sysfs path not found: {path}", file=sys.stderr)
            return False
        except Exception as e:
            print(f" Failed to write to {path}: {e}", file=sys.stderr)
            return False
    
    def read_sysfs(self, attr: str) -> Optional[str]:
        """Read from sysfs attribute"""
        path = os.path.join(self.sysfs_path, attr)
        try:
            with open(path, 'r') as f:
                return f.read().strip()
        except FileNotFoundError:
            print(f" Sysfs path not found: {path}", file=sys.stderr)
            return None
        except Exception as e:
            print(f" Failed to write to {path}: {e}", file=sys.stderr)
            return None
    
    def get_sampling_ms(self) -> int:
        val = self.read_sysfs("sampling_ms")
        return int(val) if val else 500
    
    def set_sampling_ms(self, value: int) -> bool:
        return self.write_sysfs("sampling_ms", value)
    
    def get_threshold_c(self) -> float:
        val = self.read_sysfs("threshold_mc")
        return float(val) / 1000 if val else 25.0
    
    def set_threshold_c(self, value: float) -> bool:
        return self.write_sysfs("threshold_mc", int(value * 1000))
    
    def get_mode(self) -> str:
        return self.read_sysfs("mode") or "normal"
    
    def set_mode(self, mode: str) -> bool:
        return self.write_sysfs("mode", mode)
    
    def open_device(self) -> bool:
        """Open device for polling"""
        try:
            self._fd = os.open(self.dev_path, os.O_RDONLY | os.O_NONBLOCK)
            self._poller = select.poll()
            self._poller.register(self._fd, select.POLLIN | select.POLLPRI)
            return True
        except FileNotFoundError:
            print(f" Device file not found: {self.dev_path}. Is the kernel module loaded?", file=sys.stderr)
            return False
        except Exception as e:
            print(f" Failed to open device: {e}", file=sys.stderr)
            return False
    
    def close_device(self):
        """Close device"""
        if self._fd is not None:
            os.close(self._fd)
            self._fd = None
            self._poller = None
    
    def read_sample(self) -> Optional[SensorReading]:
        """Process a reading (temperature sample)"""
        try:
            data = os.read(self._fd, SAMPLE_SIZE)

            if len(data) == 0:
                return None # EOF or temporary failure
            if len(data) < SAMPLE_SIZE:
                print(f" Short read detected ({len(data)}/{SAMPLE_SIZE} bytes). Skipping sample.", file=sys.stderr)
                return None

            # Unpack the binary data tuple (timestamp, temperature, flags)
            ts_ns, temp_mc, flags = struct.unpack(STRUCT_FORMAT, data)
            temperature = temp_mc / 1000.0
            is_alert = bool(flags & SIMTEMP_EVT_THRS)
            
            timestamp = format_timestamp(ts_ns)
            return SensorReading(timestamp, temperature, is_alert)

        except BlockingIOError:
            return None
        except Exception as e:
            print(f" Read/unpack failed: {e}", file=sys.stderr)
            return None

    def poll_reading(self, timeout_ms: int = -1, alert = False) -> Optional[SensorReading]:
        """Poll for a single reading (blocking or with timeout)"""
        if self._poller is None:
            return None
        
        self._poller.modify(self._fd, select.POLLPRI if alert else select.POLLIN)

        events = self._poller.poll(timeout_ms)
        if not events:
            return None
        
        for _, revent in events:
            if revent & select.POLLPRI:
                return self.read_sample()
            else:
                return self.read_sample()

