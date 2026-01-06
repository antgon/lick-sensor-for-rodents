![Banner](ui/banner.svg)

<h3 align="center">
Read and plot serial data from a microcontroller
</h2>

* * *

**microPyGraph** displays serial data in real time. It is intended as a
simple way to plot data from a microcontroller. It is not very efficient
in the way resources are used, as it just repeatedly reads ascii from
the serial port `print`ed line-by-line by the microcontroller, but it
helps to quickly evaluate microcontroller-acquired data values with
minimal effort.

## Requirements

* [NumPy](https://numpy.org/)
* [PyQtGraph](http://pyqtgraph.org/)
* [pySerial](https://github.com/pyserial/)


## Usage

1. Program the microcontroller for sending data over the serial port
   (typically with `print` or `printf`; see examples below) separated by
   a space at regular intervals.

1. Connect the microcontroller to the host computer.

1. Run `python main.py` on the host computer. (Even better, if `uv` is
   installed in the host computer, do `uv run main.py`.)

1. Click the "play" button to display the data.

### Example 1: Sample analog data and plot in real time

A light-dependent resistor (aka photoresistor) was connected to a
Raspberry Pico (adc pin 26) in order to measure the relative intensity
of ambient light. The following micropython code was set to read and
print the measured voltage every 500 ms:

```python
from machine import ADC
import utime

conversion_factor = 3.3/(2**16-1)

# Light-dependent resistor
ldr = ADC(26)

while True:
    ldr_adc_val = ldr.read_u16()
    ldr_voltage = ldr_adc_val * conversion_factor
    print(ldr_voltage)
    utime.sleep_ms(500)
```

Running `python microdaq.py` on the host computer and clicking the
"play" button shows the data in real time. In this example the y-axis
units are volts (as established in the micropython code above when adc
values are converted to voltage before printing).


### Example 2: Plot more than one signal

More that one signal can be plotted simultaneously by just setting the
microcontroller to print more than one value as needed. In this example,
one (analog) thermometer was connected to adc pin 28 on the same
Raspberry Pico as above. The micropython code is now:

```python
from machine import ADC
import utime

conversion_factor = 3.3/(2**16-1)

# Light-dependent resistor
ldr = ADC(26)

# Thermometer (TPM37, analog)
tmp37 = ADC(28)

while True:
    tmp37_adc_val = tmp37.read_u16()
    tmp37_voltage = tmp37_adc_val * conversion_factor
    tmp37_temperature = tmp37_voltage/0.02

    ldr_adc_val = ldr.read_u16()
    ldr_voltage = ldr_adc_val * conversion_factor

    print(tmp37_temperature, ldr_voltage)

    utime.sleep_ms(500)
```
**microPyGraph** will detect and plot the two signals. Here, the top
trace is the ambient temperature (in degrees C) and the bottom trace is
the relative ambient light intensity (in volts).


### Example 3: Using with Arduino

To test on an Arduino, the following code was uploaded to an Arduino
Uno. The two values must be printed on the same line, separated by a
space:
```c
void setup() {
  Serial.begin(9600);
}

float value = 0;
int direction = 1;
const float interval = 0.1;

void loop() {
  Serial.print(value);  // Print the first value
  Serial.print(' ');  // Print a space to separate the values
  Serial.println(sin(value));  // Print the second value and end the line
  value += (interval * direction);
  if ((value < -PI) | (value > PI)) {
    direction *= -1;
  }
  delay(50); // in ms
}
```
(This same code works for other microcontrollers that can be used with
the Arduino IDE, e.g. Adafruit Feather.)

Note that for this Arduino example to work a different baud rate may
have to be set in `main.py`. (The default baud rate in that file should
work just fine for the Raspberry Pi Pico.)
