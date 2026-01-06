/* Copyright (c) 2024-2025 Antonio González
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version. This program is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details. You
 * should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

/* Requires the `pico-mpr121` library, available at
 * https://github.com/antgon/pico-mpr121
 */
#include "mpr121.h"

/* Definitions related to the touch sensor.
 * Connect the SDA and SCL pins in the sensor to the Pico pins defined
 * here.
 */
#define MPR121_I2C_PORT i2c0
#define MPR121_I2C_PIN_SDA 20
#define MPR121_I2C_PIN_SCL 21
#define MPR121_I2C_ADDRESS 0x5A
#define MPR121_I2C_FREQ 100000

/* Define digital out pin.
 * Touch (lick) data will be written to this Pico pin. Connect this pin
 * to the data acquisition system to record licking.
 */
#define TOUCH_OUT_PIN 2

/* Touch sensor variables
 * The drinking bottle is connected to the first electrode (ELE0) in
 * the touch sensor.
 */
const uint8_t ele0 = 0;
bool ele0_is_touched;

/* Touch sensor structure */
struct mpr121_sensor mpr121;

/* Repeating timer.
 * Set up a timer to read the touch sensor at regular intervals.
*/
const int32_t sampling_interval_ms = 20;  // Sampling freq = 50 Hz
bool timer_callback(repeating_timer_t *rt);


int main() {
    stdio_init_all();

    /* Setup the default, on-board LED */
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    /* Initialise the digital output pin */
    gpio_init(TOUCH_OUT_PIN);
    gpio_set_dir(TOUCH_OUT_PIN, GPIO_OUT);
    
    /* Initialise I2C */
    i2c_init(MPR121_I2C_PORT, MPR121_I2C_FREQ);
    gpio_set_function(MPR121_I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(MPR121_I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(MPR121_I2C_PIN_SDA);
    gpio_pull_up(MPR121_I2C_PIN_SCL);

    /* Initialise the touch sensor */
    mpr121_init(MPR121_I2C_PORT, MPR121_I2C_ADDRESS, &mpr121);
    // Enable electrode 0 (The value of this function is the number of
    // electrodes to enable. Thus, a value of 1 initialises only one
    // touch electrode, i.e. electrode 0.)
    mpr121_enable_electrodes(1, &mpr121);

    /* Start repeating timer */
    repeating_timer_t timer;
    add_repeating_timer_ms(-sampling_interval_ms, timer_callback, NULL,
                           &timer);

    while(1) {
        tight_loop_contents();
    }
    return 0;
}


/* Timer callback.
 *
 * This function will be called at regular intervals (every time
 * interval as defined above). Here, the touch sensor is read and the
 * value from the sensor is passed on to the digital output pin.
 */
bool timer_callback(repeating_timer_t *rt) {
    /* Read touch status of the electrode */
    mpr121_is_touched(ele0, &ele0_is_touched, &mpr121);

    /* Write the data to the output GPIO pin */
    gpio_put(TOUCH_OUT_PIN, ele0_is_touched);

    /* The on-board LED follows touch status */
    gpio_put(PICO_DEFAULT_LED_PIN, ele0_is_touched);
}
