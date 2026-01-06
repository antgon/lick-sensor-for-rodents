#!/usr/bin/env python3
# coding=utf-8
#
# Copyright (c) 2025 Antonio González

"""
Read serial port. Expect

- nothing at all, or
- two values: timestamp, and the status of all electrodes
- save values to file

Timestamp: when the first value is received, get datetime and save to top of file. Afterwards, all timestamps are relative to the first one.

electrodes: obtain info re which electrode was triggered
by bit shifting or similar

max output: 50 Hz = 50 lines in second of this kind

4294967295 4095\n = 16 chars

= 16 * 50 = 800 chars/s
thus, reading data every 2 seconds should be fine (size of serial buffer is 4095, I think)

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
    # ""
    #Based on an example in pySerial-asyncio documentation
    #https://pyserial-asyncio.readthedocs.io/en/latest/shortintro.html
    #""
    def __init__(self):
        self.t0 = -1
        self.idx = -1
        self.idx_col = 0
        self.time_col = 1
        now = datetime.now()
        fname = f"lick_events_{now:%Y-%m-%d_%H_%M_%S}.csv"
        fname = os.path.join(output_dir, fname)
        self.fid = open(fname, mode="a")

    def connection_made(self, transport):
        self.transport = transport

    def data_received(self, data):
        self.data = np.fromstring(data, dtype = int, sep = ' ')
        self.data = self.data.reshape(-1, 3)

        # When the first lick event arrives, write to the output file the date and time; this is time 0.
        if self.t0 == -1:
            self.t0 = self.data[0, self.time_col]
            # The index column might be removed in the future; I added
            # this just to check that there is no data lost on the way
            # between the Pico and the csv file.
            self.idx = self.data[0, self.idx_col]

            # The output file receives an automatic name based on the date and time. This avoids having to ask for a file name every time the programme is run.
            start = datetime.now()
            start = f'# {start:%Y-%m-%d %H:%M:%S}\n'
            header = "idx,timestamp,electrode\n"
            self.fid.write(start)
            self.fid.write(header)
        
        # The timestamp and index of all lick events are relative to the
        # first event.
        self.data[:, self.time_col] -= self.t0
        self.data[:, self.idx_col] -= self.idx

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
        # esp. if trn on a Raspberry Pi.

        # Option 1. Save incoming data as is, without decoding electrode
        # status individually.
        # np.savetxt(self.fid, self.data, delimiter=",", fmt="%d")
        # print(self.data)

        # Option 2. Decode electrode status before saving. One line per
        # active electrode. (Thus more rows)
        for line in self.data:
            # The sensor has 12 electrodes
            for ele in range(12):
                if (line[-1] >> ele) & 0x1:
                    self.fid.write(f"{line[0]},{line[1]},{ele}\n")
                    print(line[0], line[1], ele)

        # Stop callbacks again immediately
        self.pause_reading()

        # Flush data to disk, to minimise data loss in case of a crash.
        self.fid.flush()

    def pause_reading(self):
        # Stop the callbacks to data_received
        self.transport.pause_reading()

    def resume_reading(self):
        # Start the callbacks to data_received again with all data that
        # has been received in the meantime.
        self.transport.resume_reading()

input_protocol = InputProtocol()

async def reader(port):
    transport, protocol = await create_serial_connection(
        # loop, InputProtocol, port.device, baudrate=BAUD)
        loop, lambda: input_protocol, port.device, baudrate=BAUD)

    while True:
        # Read data from serial port every these seconds
        await asyncio.sleep(2)
        protocol.resume_reading()

port = get_pico_port()
loop = asyncio.new_event_loop()
asyncio.set_event_loop(loop)

print("Acquiring lick event data. Press Ctrl+C to exit.")

try:
    loop.run_until_complete(reader(port))
except KeyboardInterrupt:
    input_protocol.fid.close()
    # pass

loop.close()

# if __name__ == "__main__":
    # app = QtWidgets.QApplication([])
    # self = MainWindow()
    # self.show()
    # sys.exit(app.exec())