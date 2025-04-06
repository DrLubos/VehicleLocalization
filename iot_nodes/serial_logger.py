"""This script logs data from serial devices connected to the system."""
import os
import re
import time
import glob
import threading
from datetime import datetime
import argparse
import serial

BAUD_RATE = 8400

DEVICE_GLOB = "/dev/cu.usbserial-*"
SCAN_INTERVAL = 2

active_loggers = {}


def extract_device_suffix(port_path):
    """
    Extracts the device suffix from a given port path.

    This function searches for a pattern in the port path that matches
    'usbserial-<suffix>' and extracts the suffix. If no match is found,
    it returns 'UNKNOWN'.

    Args:
        port_path (str): The path of the port as a string.

    Returns:
        str: The extracted device suffix if found, otherwise 'UNKNOWN'.
    """
    match = re.search(r'usbserial-(\w+)', port_path)
    return match.group(1) if match else 'UNKNOWN'


def create_log_filename(port_path):
    """
    Generates a log filename based on the current timestamp and a device-specific suffix.

    The filename is created in the format: `<timestamp>_<device_suffix>.log`, where:
    - `<timestamp>` is the current date and time in the format `YYYY-MM-DD_HH-MM-SS`.
    - `<device_suffix>` is extracted from the `port_path` using `extract_device_suffix` function.

    The log file is stored in a directory named `logs` which is created if it does not exist.

    Args:
        port_path (str): The path of the device port used to extract the device-specific suffix.

    Returns:
        str: The full path to the generated log file.
    """
    now = datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
    suffix = extract_device_suffix(port_path)
    filename = f"{now}_{suffix}.log"
    log_dir = os.path.join("logs")
    os.makedirs(log_dir, exist_ok=True)
    return os.path.join(log_dir, filename)


def log_serial_data(port_path, stop_event):
    """
    Logs data from a serial port to a file with timestamps.

    This function reads data from the specified serial port and writes it to a log file.
    Each line of data is prefixed with a UTC timestamp. The logging continues until the
    provided stop_event is set.

    Args:
        port_path (str): The path to the serial port (e.g., '/dev/ttyUSB0').
        stop_event (threading.Event): An event used to signal when to stop logging.

    Raises:
        Exception: If the serial port cannot be opened or if there is an error during reading.

    Notes:
        - The serial port is opened with a defined baud rate and a timeout of 1 second.
        - The log file is created using a helper function `create_log_filename` which
          generates a filename based on the port path.
        - Any decoding errors in the serial data are replaced with a placeholder character.
    """
    try:
        with serial.Serial(port_path, BAUD_RATE, timeout=1) as ser:
            log_path = create_log_filename(port_path)
            with open(log_path, 'w', encoding="utf-8") as logfile:
                print(f"[{port_path}] ➜ logging to {log_path}")
                while not stop_event.is_set():
                    try:
                        line = ser.readline().decode('utf-8', errors='replace').strip()
                        if line:
                            timestamp = datetime.utcnow().isoformat()
                            logfile.write(f"{timestamp} {line}\n")
                            logfile.flush()
                    except (serial.SerialException, OSError, UnicodeDecodeError) as e:
                        print(f"[{port_path}] read error: {e}")
                        break
    except (serial.SerialException, OSError) as e:
        print(f"[{port_path}] failed to open: {e}")
    finally:
        print(f"[{port_path}] stopped logging.")


def device_scanner():
    """
    Continuously scans for connected devices on serial ports and manages logging threads.

    This function monitors serial ports matching a specific pattern (defined by DEVICE_GLOB) 
    and detects when devices are connected or disconnected. For each newly connected device, 
    it starts a logging thread to handle serial data. When a device is disconnected, 
    it stops the corresponding logging thread.

    Globals:
        DEVICE_GLOB (str): A pattern used to match serial device ports.
        SCAN_INTERVAL (float): The interval (in seconds) between scans for connected devices.
        active_loggers (dict): A dictionary mapping port names to their corresponding 
            logging thread and stop event.

    Behavior:
        - Detects newly connected devices and starts a logging thread for each.
        - Detects disconnected devices and stops their corresponding logging threads.
        - Continuously updates the set of known ports.

    Note:
        This function runs indefinitely and should be executed in a separate thread or process 
        to avoid blocking the main program.
    """
    known_ports = set()
    while True:
        current_ports = set(glob.glob(DEVICE_GLOB))

        for port in current_ports - known_ports:
            print(f"Connected: {port}")
            stop_event = threading.Event()
            thread = threading.Thread(target=log_serial_data, args=(port, stop_event), daemon=True)
            active_loggers[port] = (thread, stop_event)
            thread.start()

        for port in known_ports - current_ports:
            print(f"Disconnected: {port}")
            if port in active_loggers:
                _, stop_event = active_loggers.pop(port)
                stop_event.set()

        known_ports = current_ports
        time.sleep(SCAN_INTERVAL)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Serial logger for vehicleLocalization")
    parser.add_argument("-b", "--baud", type=int, default=38400,
                        help="Set baud rate (default: 38400)")
    args = parser.parse_args()
    BAUD_RATE = args.baud
    print(f"Using baud rate: {BAUD_RATE}")
    print("Waiting for devices...")
    try:
        device_scanner()
    except KeyboardInterrupt:
        print("\nKeyboard interrupt received. Exiting...")
        exit(0)
