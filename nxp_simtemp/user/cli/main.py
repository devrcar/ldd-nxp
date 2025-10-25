#!/usr/bin/env python3
"""CLI tool using shared backend"""
import sys
import argparse
from backend.simtemp_interface import SimTempSensorInterface

def monitor_readings(sensor):
    if not sensor.open_device(): return 1
    try:
        while True:
            reading = sensor.poll_reading()
            if reading:
                print(reading)
    except KeyboardInterrupt:
        print("\n--- Monitor stopped by user. ---")
        ret = 0
    except Exception as e:
        print(f"\n Unhandled error in monitor: {e}", file=sys.stderr)
        ret = 1
    finally:
        sensor.close_device()
        return ret

def run_test_mode(sensor):
    if not sensor.open_device(): return 1

    # First getting all the enqueued data
    try:
        while True:
            reading = sensor.read_sample()
            if reading:
                print(reading)
            else:
                break
    except Exception as e:
        print(f"\n Unhandled error in test mode: {e}", file=sys.stderr)
        return 1

    # Then switch to test mode
    print("\n--- Entering in Test Mode ---")

    alert = True
    new_threshold = 24.5 # (assuming normal temp is about 25C)
    timeout = 2 * sensor.get_sampling_ms() # Wait for up to 2 periods for the alert

    print(f"--- Test timeout:  {timeout} ms ---")
    print(f"--- Test threshold:  {new_threshold} C ---")
    print(f"--- Test mode: normal ---")

    old_threshold = sensor.get_threshold_c()
    sensor.set_threshold_c(new_threshold)
    sensor.set_mode("normal")

    print("--- Polling ---")
    reading = sensor.poll_reading(timeout, alert)

    sensor.set_threshold_c(old_threshold) # restoring initial value
    sensor.close_device()

    if reading and reading.is_alert:
        print(f"{reading}\n")
        print("✓ Test PASSED")
        return 0
    else:
        print("✗ Test FAILED")
        return 1

def main():
    parser = argparse.ArgumentParser(description="Simtemp sensor CLI")
    parser.add_argument('--sampling-ms', type=int)
    parser.add_argument('--threshold', type=float, help="Value in C degrees [float]")
    parser.add_argument('--mode', choices=['normal', 'noisy', 'ramp'])
    parser.add_argument('--monitor', action='store_true')
    parser.add_argument('--test', action='store_true')
    args = parser.parse_args()
    
    sensor = SimTempSensorInterface()
    
    # Configure
    if args.sampling_ms:
        sensor.set_sampling_ms(args.sampling_ms)
    if args.threshold:
        sensor.set_threshold_c(args.threshold)
    if args.mode:
        sensor.set_mode(args.mode)
    
    # Test mode
    if args.test:
        ret_code = run_test_mode(sensor)
        sys.exit(ret_code)

    # Monitor
    elif args.monitor or not any([args.sampling_ms, args.threshold, args.mode]):
        ret_code = monitor_readings(sensor)
        sys.exit(ret_code)

    return 0

if __name__ == "__main__":
    sys.exit(main())
