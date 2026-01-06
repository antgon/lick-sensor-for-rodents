#!/usr/bin/env python3
# coding=utf-8
#
# Copyright (c) 2025 Antonio González

""" lick_events_reader.py

Read lick events detected and sent over serial by the lick sensor

The lick sensor can be programmed to detect lick events and print to
serial the timestamp and sensor information every time a lick is
detected.

This script is for reading those lick events and saving them to a text
file.

Usage
-----
* Each line of data sent over serial is expected to consist of the
  timestamp when a lick event was detected, followed by one or more
  sensor values.
* Define the header below, to match the number of columns of data to
  expect from the lick sensor.
* Connect the Pico to the computer before running this script.
* This script will create a text file and then wait for data to be sent
  over serial.
* Every few cycles, the data values received will be written to a csv
  file.
* The received sensor value is a binary representation of the electrodes
  in the sensor where a lick was detected. Here, those values are stored
  as such to the csv file.
* Alternatively, it is possible to decode the sensor values into
  electrode on/off values. This may take more computer effort during
  acquisition and I do not know if it is a good idea, especially on the
  Raspberry Pi. At this stage I prefer to acquire data as quick as
  possible, and only extract the information offline.

last modified: 2025-02-27
"""

"""
I tested this script on the Raspberry Pi 5 (8G), with the Pico sending
all data (50 Hz) from 2 sensors regardless of touch detection, reading
data once every second, and flushing data to file every 10 seconds).

The Raspberry did not complain at all. After ~5 minutes, the data file
created was

* 15073 lines (makes sense: 5 min @50Hz = 15000 samples)
* Size, 160722
* Thus, a 5-hour (300 min) experiment should generate a file of this
  size: 300 * 160722 / 5 = 9417K = 9.2M

Also, during acquisition, I opened Task Manager and found that the CPU
and memory usage was negligible: memory used was ~520 MB and went down
to 500 MB after quitting. CPU was at all times hovering around 1%.

This all indicates that it should be perfectly possible to run
experiments with the lick sensor for many hours.
"""

import asyncio
from datetime import datetime
import os
import sys
import time

import numpy as np

from serial import Serial, SerialException
from serial.tools import list_ports
from serial_asyncio import create_serial_connection

# Parameters
BAUD = 115200
HEADER = "timestamp,sensorA,sensorB\n"
output_dir = "."  # os.path.expanduser("~")


def get_pico_port():
    port = [p for p in list_ports.grep("Pico")]
    if len(port) == 1:
        return port[0]
    elif len(port) == 0:
        raise SerialException("no Pico was found")
    elif len(port) > 1:
        # When would this happen?
        raise SerialException("there is more than one Pico")


class InputProtocol(asyncio.Protocol):
    """
    Based on an example in pySerial-asyncio documentation
    https://pyserial-asyncio.readthedocs.io/en/latest/shortintro.html
    """
    def __init__(self):
        self.t0 = -1
        self.ncols = len(HEADER.split(','))
        self.counter = 0
        # self.idx = -1
        # self.idx_col = 0
        self.time_col = 0
        # The output file receives an automatic name based on the date
        # and time. This avoids having to ask for a file name every time
        # the programme is run.
        now = datetime.now()
        fname = f"lick_events_{now:%Y-%m-%d_%H_%M_%S}.csv"
        fname = os.path.join(output_dir, fname)
        self.fid = open(fname, mode="a")

    def connection_made(self, transport):
        self.transport = transport

    def data_received(self, data):
        # print(data)
        self.data = np.fromstring(data, dtype=int, sep=' ')
        try:
            self.data = self.data.reshape(-1, self.ncols)
        except ValueError as err:
            print(err)
            print("Ignoring values:", self.data)
            self.data = np.zeros(self.ncols, dtype=int).reshape(
                -1, self.ncols)

        # When the first lick event arrives, write to the output file
        # the date and time; this is time 0.
        if self.t0 == -1:
            self.t0 = self.data[0, self.time_col]
            start = datetime.now()
            start = f'# {start:%Y-%m-%d %H:%M:%S}\n'
            self.fid.write(start)
            self.fid.write(HEADER)
        
        # The timestamp and index of all lick events are relative to the
        # first event.
        self.data[:, self.time_col] -= self.t0
        # self.data[:, self.idx_col] -= self.idx

        # Electrode data, as received from the Pico, codes on/off in a
        # binary form, as one single number. Thus if electrodes 0 and 4
        # are active, the number will be 17 (0b10001), etc.
        #
        # This value can be saved directly to the csv file, to be
        # decoded off-line later, or the status of each electrode can be
        # extracted here before saving.
        #
        # I do not know if the second option would be more considerably
        # more memory/time intensive and thus slow down the process,
        # esp. if on a Raspberry Pi.

        # Option 1. Save incoming data as is, without decoding electrode
        # status individually.
        np.savetxt(self.fid, self.data, delimiter=",", fmt="%d")
        # print(self.data)

        # Option 2. Decode electrode status before saving. One line per
        # active electrode. (Thus more rows)
        # for line in self.data:
            # The sensor has 12 electrodes
            # for ele in range(12):
                # if (line[-1] >> ele) & 0x1:
                    # self.fid.write(f"{line[0]},{line[1]},{ele}\n")
                    # print(line[0], line[1], ele)

        # Stop callbacks again immediately
        self.pause_reading()

        # Every few cycles flush data to disk, to minimise data loss in
        # case of a crash.
        self.counter += 1
        if self.counter == 10:
            self.fid.flush()
            self.counter = 0

    def pause_reading(self):
        # Stop the callbacks to data_received
        self.transport.pause_reading()

    def resume_reading(self):
        # Start the callbacks to data_received again with all data that
        # has been received in the meantime.
        self.transport.resume_reading()

port = get_pico_port()
input_protocol = InputProtocol()

async def reader(port):
    transport, protocol = await create_serial_connection(
        loop, lambda: input_protocol, port.device, baudrate=BAUD)
        # loop, InputProtocol, port.device, baudrate=BAUD)

    while True:
        # Read data from serial port every these seconds. I've found
        # that setting this to 2 seconds is not fast enough and the
        # programme will crash. A value of 1 second seems to be okay.
        await asyncio.sleep(1)
        protocol.resume_reading()

loop = asyncio.new_event_loop()
asyncio.set_event_loop(loop)

print("Acquiring lick event data. Press Ctrl+C to exit.")

try:
    loop.run_until_complete(reader(port))
except KeyboardInterrupt:
    input_protocol.fid.close()
    sys.exit()

loop.close()

