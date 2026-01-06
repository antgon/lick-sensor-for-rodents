/* Copyright (c) 2025 Antonio González
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

/* lick_two_sensors.c

   Two MPR121 touch sensors are connected to the Pico. This allows
   detecting licks in up to 24 drinking bottles simultaneously. When a
   lick is detected, the timestamp and electrode data are printed to
   serial.
 */


#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

/* Requires the pico-mpr121 libary which is available at
 * https://github.com/antgon/pico-mpr121
 */
#include "mpr121.h"

/* Touch sensor I2C definitions
 * The SDA and SCL pins in the sensors are connected to the Pico pins
 * defined here.
 */
// Shared I2C definitions. Both sensors are connected to the same pair
// of pins
#define MPR121_I2C_PORT i2c0
#define MPR121_I2C_FREQ 400000
#define MPR121_I2C_PIN_SDA 20
#define MPR121_I2C_PIN_SCL 21
// I2C addresses for sensor A and B
#define MPR121_A_I2C_ADDRESS 0x5A
#define MPR121_B_I2C_ADDRESS 0x5B

/* Touch sensor variables */
uint16_t is_touched_A = 0;
uint16_t was_touched_A = 0;
uint16_t is_onset_A = 0;
uint16_t is_touched_B = 0;
uint16_t was_touched_B = 0;
uint16_t is_onset_B = 0;

/* Touch sensor structures */
struct mpr121_sensor mpr121_A;
struct mpr121_sensor mpr121_B;

/* Time variables */
absolute_time_t now;
uint32_t timestamp;

/* Repeating timer */
const int32_t sampling_interval_ms = 20;  // 50 Hz
bool timer_callback(repeating_timer_t *rt);


void mpr121_get_noise_half_delta(uint8_t *rising, uint8_t *falling,
        uint8_t *touched, mpr121_sensor_t *sensor) {
    // Read NHD values
    mpr121_read(MPR121_NOISE_HALF_DELTA_RISING_REG, rising, sensor);
    mpr121_read(MPR121_NOISE_HALF_DELTA_FALLING_REG, falling, sensor);
    mpr121_read(MPR121_NOISE_HALF_DELTA_TOUCHED_REG, touched, sensor);
}

// uint8_t nclr;
// uint8_t nclf;
// uint8_t nclt;
// void mpr121_get_noise_count_limit(uint8_t *rising, uint8_t *falling,
        // uint8_t *touched, mpr121_sensor_t *se nsor) {
    // Read NCL values
    // mpr121_read(MPR121_NOISE_COUNT_LIMIT_RISING_REG, rising, sensor);
    // mpr121_read(MPR121_NOISE_COUNT_LIMIT_FALLING_REG, falling, sensor);
    // mpr121_read(MPR121_NOISE_COUNT_LIMIT_TOUCHED_REG, touched, sensor);
// }

uint16_t oor = 0;
void mpr121_get_out_of_range_status(uint16_t *oor,
        mpr121_sensor_t *sensor) {
    uint8_t reg;
    mpr121_read(MPR121_OUT_OF_RANGE_STATUS_0_REG, &reg, sensor);
    *oor |= reg;
    mpr121_read(MPR121_OUT_OF_RANGE_STATUS_1_REG, &reg, sensor);
    *oor |= (reg << 8);
}


int main() {
    stdio_init_all();

    /* Setup the default, on-board LED */
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    /* Initialise I2C */
    i2c_init(MPR121_I2C_PORT, MPR121_I2C_FREQ);
    gpio_set_function(MPR121_I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(MPR121_I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(MPR121_I2C_PIN_SDA);
    gpio_pull_up(MPR121_I2C_PIN_SCL);

    /* Initialise the touch sensors */
    mpr121_init(MPR121_I2C_PORT, MPR121_A_I2C_ADDRESS, &mpr121_A);
    mpr121_init(MPR121_I2C_PORT, MPR121_B_I2C_ADDRESS, &mpr121_B);

    /* Enable all electrodes (0 to 11, thus enable n=12) */
    mpr121_enable_electrodes(12, &mpr121_A);
    mpr121_enable_electrodes(12, &mpr121_B);

    /* Sensor settings */
    
    // Thresholds (touch, release)
    // Default: 15, 10
    const uint8_t tth = 15;
    const uint8_t rth = 10;
    mpr121_set_thresholds(tth, rth, &mpr121_A);
    mpr121_set_thresholds(tth, rth, &mpr121_B);
    
    // Max half delta (rising, falling). Range 1~63
    // Default: 1, 1
    const uint8_t mhdr = 1;
    const uint8_t mhdf = 1;
    mpr121_set_max_half_delta(mhdr, mhdf, &mpr121_A);
    mpr121_set_max_half_delta(mhdr, mhdf, &mpr121_B);
    
    // Noise half delta (rising, falling, touched). Range 1~63
    // Peppe's sensor, 1, 1 ,3
    // Default: 1, 1, 1
    const uint8_t nhdr = 1;
    const uint8_t nhdf = 1;
    const uint8_t nhdt = 1;
    mpr121_set_noise_half_delta(nhdr, nhdf, nhdt, &mpr121_A);
    mpr121_set_noise_half_delta(nhdr, nhdf, nhdt, &mpr121_B);
    
    // Noise count limit (rising, falling, touched). Range 0~255.
    // Default: 0, 255, 0
    const uint8_t nclr = 0;
    const uint8_t nclf = 0;
    const uint8_t nclt = 0;
    mpr121_set_noise_count_limit(nclr, nclf, nclt, &mpr121_A);
    mpr121_set_noise_count_limit(nclr, nclf, nclt, &mpr121_B);
    
    // Filter delay limit (rising, falling, touched). Range 0~255.
    // Default: 0, 2, 0
    const uint8_t fdlr = 0;
    const uint8_t fdlf = 0;
    const uint8_t fdlt = 0;
    mpr121_set_filter_delay_limit(fdlr, fdlf, fdlt, &mpr121_A);
    mpr121_set_filter_delay_limit(fdlr, fdlf, fdlt, &mpr121_B);

    // Debounce (touch, release). Range 0~7.
    // Default: 0, 0
    const uint8_t tdbnc = 0;
    const uint8_t rdbnc = 0;
    mpr121_set_debounce(tdbnc, rdbnc, &mpr121_A);
    mpr121_set_debounce(tdbnc, rdbnc, &mpr121_B);
    
    /* Start repeating timer */
    repeating_timer_t timer;
    add_repeating_timer_ms(-sampling_interval_ms, timer_callback, NULL,
                           &timer);

    // FOR TESTING ONLY.
    // sleep_ms(5000);

    // mpr121_get_out_of_range_status(&oor, &mpr121_A);
    // printf("Out of range A: %016b\n", oor);

    // mpr121_get_out_of_range_status(&oor, &mpr121_B);
    // printf("Out of range B: %016b\n", oor);
    
    // mpr121_get_noise_half_delta(&nhdr, &nhdf, &nhdt, &mpr121_A);
    // printf("NHD: %u %u %u\n", nhdr, nhdf, nhdt);

    // mpr121_get_noise_count_limit(&nclr, &nclf, &nclt, &mpr121_A);
    // printf("NCL: %u %u %u\n", nclr, nclf, nclt);
    
    // END TESTING

    while(1) {
        tight_loop_contents();
    }
    return 0;
}


/* Timer callback
 *
 * This function will be called at every time interval defined above. 
 * The touch sensors are read. If a lick is detected, the timestamp and
 * electrode are printed to serial.
 */
bool timer_callback(repeating_timer_t *rt) {
    // Read the sensors.
    mpr121_touched(&is_touched_A, &mpr121_A);
    mpr121_touched(&is_touched_B, &mpr121_B);

    // The on-board LED follows touch status in sensor A0.
    gpio_put(LED_PIN, is_touched_A & 0x1);

    // Determine if there was a change in status from 0 to 1 in any
    // electrode. This indicates the onset of a lick event.
    is_onset_A = (was_touched_A ^ is_touched_A) & is_touched_A;
    is_onset_B = (was_touched_B ^ is_touched_B) & is_touched_B;

    // If a lick was detected by any sensor, print the timestamp and
    // sensor data to serial.
    if (is_onset_A || is_onset_B) {
        // Print sensor data. One single number per sensor represents
        // the status (on/off) of the full set of 12 electrodes.
        now = get_absolute_time();
        timestamp = to_ms_since_boot(now);
        printf("%u %u %u\n", timestamp, is_onset_A, is_onset_B);

        // As an alternative to the line above, for debugging it helps
        // to see the binary representation of the pins touched.
        // printf("%u A%012b B%012b\n", timestamp, is_onset_A,
        // is_onset_B);
    }

    // Uncomment to test the reader
    //
    // To test the ability of the reader to receive and handle the data,
    // comment out the if statement above, and instead write sensor
    // values regardless of whether a touch event was detected or not.
    // now = get_absolute_time();
    // timestamp = to_ms_since_boot(now);
    // printf("%u %u %u\n", timestamp, is_onset_A, is_onset_B);

    was_touched_A = is_touched_A;
    was_touched_B = is_touched_B;
}
