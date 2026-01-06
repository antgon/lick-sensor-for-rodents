#!/usr/bin/env python3
# coding=utf-8
#
# Copyright (c) 2023-2025 Antonio González

# Data: Baseline, filtered data, touch and release thresholds, status
# One panel. Data in: n = 5
# Curves: baseline, data, thresholds (green, red, ...
# Infinite lines: on/off (yellow, white)

"""
, a touch is defined as any time the difference between the Filtered Data and the Baseline Value is greater than
the threshold

For the system to recognize a touch the delta must be greater than the Touch Threshold.
Delta = Baseline - Data
Trigger Touch → Delta > Touch Threshold

A release is triggered when the Delta falls below the Release Threshold
Trigger Release → Delta < Touch Threshold

python3-pyqtgraph
python3-pyqt6
"""
from collections import deque
from datetime import datetime
from itertools import count
import os
import sys
import time

import numpy as np
from PyQt6 import (QtCore, QtGui, QtWidgets)
from PyQt6.QtCore import pyqtSlot

import pyqtgraph as pg
from serial import Serial, SerialException
from serial.tools import list_ports

from ui.ui_main import Ui_MainWindow

# GUI parameters
GUI_REFRESH_RATE = 100  # In milliseconds
WIN_WIDTH_SAMPLES = 150
CURVE_WIDTH = 2

# List of signals as expected to arrive in the serial port from the
# Pico. Each signal is `panel` (panel index where this signal will be
# plotted), `name` name of the signal, `colour` for the curve.
signals = {
    # Each signal entry is: panel, name, colour
    0: [0, "Baseline", "red"],
    1: [0, "Signal", "green"],
    2: [1, "Delta", "orange"],
    3: [1, "TTH", "red"],
    4: [1, "RTH", "green"],
    5: [1, "Status", "yellow"]}
        
# Serial parameters
BAUD_DEFAULT = 115200
BAUD_RATES = (9600, 19200, 38400, 57600, 115200)

pg.setConfigOption('background', 'lightgrey')


class Settings:
    def __init__(self):
        # Connection settings
        self.baud = BAUD_DEFAULT
        self.port = ''
        self.scan_ports()
        self.first_is_x = False
        if len(self.available_ports) > 0:
            self.port = self.available_ports[0].device

        # UI settings
        self.width = WIN_WIDTH_SAMPLES

        # Path for saving data
        self.save_path = os.path.expanduser("~")

    def scan_ports(self):
        self.available_ports = list_ports.comports()
        for port in self.available_ports:
            if port.manufacturer is None:
                self.available_ports.remove(port)
        self.available_ports.sort()


class MainWindow(QtWidgets.QMainWindow, Ui_MainWindow):
    """
    Data acquisition main window
    """
    def __init__(self, parent=None):
        QtWidgets.QWidget.__init__(self, parent)
        self.setupUi(self)
        self._gui_refresh_rate = GUI_REFRESH_RATE
        self.playButton.setEnabled(True)
        self.stopButton.setEnabled(False)
        self.settings = Settings()

        # Create a timer to update the plot at regular intervals
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.update)

    @pyqtSlot()
    def on_quitButton_clicked(self):
        self.stop()
        self.close()

    @pyqtSlot()
    def on_playButton_clicked(self):
        self.start()

    @pyqtSlot()
    def on_stopButton_clicked(self):
        self.stop()

    def start(self, retry=3):
        """
        Start reading serial data
        """
        # Connect to the microcontroller and wait for data
        self.statusbar.showMessage('Connecting to µC...')
        try:
            self.serial = Serial(port=self.settings.port,
                                 baudrate=self.settings.baud,
                                 timeout=None)
        except SerialException as exc:
            self.stop()
            self.statusbar.showMessage("Serial error")
            QtWidgets.QMessageBox.critical(self, "Serial error",
                                           exc.strerror)
            return
        time.sleep(0.1)

        retries = 0
        self.statusbar.showMessage("Waiting for data...")
        self.serial.reset_input_buffer()
        while self.serial.in_waiting < 30:
            if retries == retry:
                msg = "No serial data received."
                self.stop()
                QtWidgets.QMessageBox.information(self, "Notice", msg)
                return
            retries += 1
            time.sleep(1)


        # Read the first line of data. When the wrong baud rate is set
        # this line will not have an end-of-line character, and that can
        # be used to alert the user. (I do not know if this will always
        # work out though.)
        #
        # Note that, according to pySerial documentation
        # (https://pythonhosted.org/pyserial/shortintro.html#readline),
        # an exception should be raised if readline() does not find an
        # end-of-line when a timeout is set. However, that does not work
        # for me: even with a timeout, readline() blocks forever when
        # the wrong baud rate is set (and thus no eol is found). These
        # lines seem to do a good job at circumventing that issue.
        self.serial.timeout = 0
        line = self.serial.readline(30)
        if not line.endswith(b'\n'):
            self.stop()
            msg = ("The serial stream is not as expected.\n" +
                   "Perhaps the wrong baud rate was set?")
            QtWidgets.QMessageBox.critical(self, "Serial error", msg)
            return

        # If the first line was successfully read, this can be used to
        # find out how many values per line (i.e. signals) there are in
        # the data.
        #line = line.decode().split()

        # Initialise data containers.
        #if self.settings.first_is_x:
            #nsignals = len(line)-1
            ## First time value, convert from ms to s.
            #self._x0 = float(line[0])/1000
        #else:
            #nsignals = len(line)
            #self._x0 = 0
        # For the purpose of testing the sensor we hardcode 6 signals:
        # baseline, data, delta, tth, rth, is_touched
        nsignals = len(signals)
        # self._x0 = 0
        self.data = []

        # The first set of values in data is values of x. The following
        # sets are each of the signals.
        # for _ in range(nsignals+1):
        for _ in range(nsignals):
            self.data.append(deque([], maxlen=self.settings.width))

        # Set up plots
        self.setup_plot(nsignals)

        # Start the timer
        self.serial.timeout = None
        self.timer.start(self._gui_refresh_rate)

        # Update gui
        self.statusbar.clearMessage()
        self.playButton.setEnabled(False)
        self.stopButton.setEnabled(True)
        # self.recButton.setEnabled(True)
        # self.settingsButton.setEnabled(False)

    def stop(self):
        """
        Stop reading serial data.
        """
        # Stop data acquisition.
        self.timer.stop()

        # Close serial connection
        if hasattr(self, 'serial'):
            self.serial.close()

        # if self.recButton.isChecked():
            # self.recButton.toggle()

        # Reset gui
        self.statusbar.clearMessage()
        self.playButton.setEnabled(True)
        self.stopButton.setEnabled(False)
        # self.recButton.setEnabled(False)
        # self.settingsButton.setEnabled(True)

    def update(self):
        """
        Update the plots with incoming data

        This function runs repeatedly under a QTimer. It reads the data
        form the serial port and plots it.
        """
        if self.serial.in_waiting > 10:
            try:
                this_data = self.serial.readlines(self.serial.in_waiting)
                for line in this_data:
                    line = line.decode()

                    # Write data to file if requested.
                    #if self.recButton.isChecked():
                        #self._outfile.writelines(line)
                        #self._outfile.flush()

                    # Push the data values in to the data queues. The
                    # first data queue is x. This can be time values
                    # read from the serial input or the index of the
                    # sample.
                    #if self.settings.first_is_x:
                        #for (index, val) in zip(count(), line.split()):
                            #val = float(val)
                            #if index == 0:
                                ## Convert from ms to s, then substract
                                ## first time value so that sampling
                                ## always starts at time 0.
                                #val = (val/1000) - self._x0
                            #self.data[index].append(val)
        
                    # Data: Baseline, filtered data, touch and release
                    # thresholds, status
                    #else:
                    for (index, val) in zip(count(), line.split()):
                        #if index == 0:
                            #self.data[0].append(self._x0)
                            #self._x0 += 1
                        #self.data[index+1].append(float(val))
                        val = float(val)
                        # if index == 5:
                            # val *= 270
                            #l = pg.InfiniteLine(pos=float(val), angle=90,
                                            #pen=None, movable=False)
                            #self.plots[-1].addItem(l)
                        self.data[index].append(val)
                        self.curves[index].setData(y=self.data[index])

                # Plot the data. First data queue is x.
                #for (index, samples) in zip(count(), self.data[1:-1]):
                    #self.curves[index].setData(self.data[0], samples)

            except ValueError as error:
                print(error)
                self.timer.stop()
                # sys.exit()

    def setup_plot(self, nsignals):
        # title_fontsize = 10
        x_tick_fontsize = 10
        y_tick_fontsize = 10
        # marker_fontsize = 8
        y_tick_margin = 60

        xfont = pg.QtGui.QFont()
        yfont = pg.QtGui.QFont()
        yfont.setPointSize(y_tick_fontsize)
        xfont.setPointSize(x_tick_fontsize)

        # Format with bounding box...
        # self.layout = pg.GraphicsLayout(border=(100, 100, 100))
        # ...or no box.
        self.layout = pg.GraphicsLayout()

        self.graphicsView.setCentralItem(self.layout)
        self.plots = []
        self.curves = []

        # Create 2 plots.
        for nrow in [0, 1]:
            plot = self.layout.addPlot(row=nrow, col=0)
            # Format y-axis.
            # Add a fixed margin to the left so that the plots are
            # aligned regardless of the width of the y-ticklabels.
            yaxis = plot.axes['left']['item']
            yaxis.setWidth(y_tick_margin)
            yaxis.setTickFont(yfont)

            # Format x-axis. Do not show x-ticklabels but do retain
            # the x-axis line and the vertical grid lines.
            xaxis = plot.axes['bottom']['item']
            xaxis.setStyle(showValues=False)
            plot.showGrid(x=True, y=True)
            
            plot.setLabel('bottom', ' ', size=10)
            plot.setXRange(0, self.settings.width)
            plot.addLegend()
            self.plots.append(plot)

        # Create the curves
        for nrow in range(nsignals):
            # Create curves.
            # curve = plot.plot(pen=self.settings.curve_colour)
            signal = signals[nrow]
            panel, name, colour = signal
            pen = pg.mkPen(colour, width=CURVE_WIDTH)
            plot = self.plots[panel]
            curve = plot.plot(pen=pen, name=name)
            #self.plots.append(plot)
            self.curves.append(curve)
            # plot.setXRange(0, self.settings.width)

            # An empty label help to add some space between the plots.
            # Otherwise the last plot is shorter than the rest.
            #plot.setLabel('bottom', ' ', size=10)

        # Link x-axis from all plots to that of the last one.
        for plot in self.plots[:-1]:
            plot.setXLink(self.plots[-1])


if __name__ == "__main__":
    app = QtWidgets.QApplication([])
    self = MainWindow()
    self.show()
    sys.exit(app.exec())
