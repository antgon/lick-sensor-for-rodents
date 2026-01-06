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

// Example of a lick sensor using 3 bottles.

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

/* Requires the pico-mpr121 libary which is available at
 * https://github.com/antgon/pico-mpr121
 */
#include "mpr121.h"

/* Touch sensor I2C definitions
 * Connect the SDA and SCL pins in the sensor to the Pico pins defined
 * here.
 */
#define MPR121_I2C_PORT i2c0
#define MPR121_I2C_PIN_SDA 20
#define MPR121_I2C_PIN_SCL 21
#define MPR121_I2C_ADDRESS 0x5A
#define MPR121_I2C_FREQ 100000

/* Define digital out pin
 * Touch (lick) data will be written to this Pico pin. Connect this pin
 * to the data acquisition system to record licking.
 */
#define OUT_PIN_0 2
#define OUT_PIN_1 4
#define OUT_PIN_2 6
#define OUT_PIN_3 8
#define OUT_PIN_4 11
#define OUT_PIN_5 13
uint32_t gpio_mask = (1 << OUT_PIN_0) | (1 << OUT_PIN_1) |
    (1 << OUT_PIN_2) | (1 << OUT_PIN_3) | (1 << OUT_PIN_4) |
    (1 << OUT_PIN_5);

/* Touch sensor variables
 * Six electrodes are used: ELE0 to ELE5.
 */
uint8_t n_ele = 6;
uint16_t is_touched;

/* Touch sensor structure */
struct mpr121_sensor mpr121;

/* Repeating timer.
 * This timer will trigger at regular intervals the function to read the
 * the touch sensor.
*/
const int32_t sampling_interval_ms = 20;  // 50 Hz
bool timer_callback(repeating_timer_t *rt);


int main() {
    stdio_init_all();

    gpio_init_mask(gpio_mask);
    gpio_set_dir_out_masked(gpio_mask);
     
    /* Initialise I2C */
    i2c_init(MPR121_I2C_PORT, MPR121_I2C_FREQ);
    gpio_set_function(MPR121_I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(MPR121_I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(MPR121_I2C_PIN_SDA);
    gpio_pull_up(MPR121_I2C_PIN_SCL);
   
    /* Initialise the touch sensor */
    mpr121_init(MPR121_I2C_PORT, MPR121_I2C_ADDRESS, &mpr121);
    // Enable the first two electrodes (ELE0 and ELE1)
    mpr121_enable_electrodes(n_ele, &mpr121);
    
    /* Start repeating timer */
    repeating_timer_t timer;
    add_repeating_timer_ms(-sampling_interval_ms, timer_callback, NULL,
                           &timer);

    while(1) {
        tight_loop_contents();
    }
    return 0;
}


/* Timer callback
 *
 * This function will be called repeatedly (every time interval as
 * defined above). Here, the touch sensor is read and the value from the
 * sensor is passed on to the digital output pin.
 */
bool timer_callback(repeating_timer_t *rt) {
    /* Check electrodes touch status */
    mpr121_touched(&is_touched, &mpr121);
    
    /* Write the data to the output pins */
    gpio_put_masked(gpio_mask, (uint32_t)is_touched);
}
