/*
 * Copyright (c) 2021-2025 Antonio González
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


/* MPR121 test sensor settings.

In this example only one touch sensor (electrode 0) is enabled. Then,
this electrode is polled at regular intervals and several values
reported by the sensor, notably the baseline and filtered values, are
`printf`ed to serial. These values can be read and plotted in real time
with the acompanying Python script `plotter.py`.

The various sensor settings that can be altered are exposed below. The
idea is thus to be able to play around with any of these settings and
observe how the sensor reacts differently to touch depending on these
settings.
*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "mpr121.h"

// I2C definitions: port and pin numbers
#define I2C_PORT i2c0
#define I2C_SDA 20
#define I2C_SCL 21

// MPR121 I2C definitions: address and frequency.
#define MPR121_ADDR 0x5A
#define MPR121_I2C_FREQ 400000

int main() {
    stdio_init_all();

    // Setup the default, on-board LED.
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Initialise I2C.
    i2c_init(I2C_PORT, MPR121_I2C_FREQ);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Initialise and autoconfigure the touch sensor.
    struct mpr121_sensor mpr121;
    mpr121_init(I2C_PORT, MPR121_ADDR, &mpr121);
    
    // Enable only one touch sensor (electrode 0).
    mpr121_enable_electrodes(1, &mpr121);

    // Initialise data variables.
    const uint8_t electrode = 0;
    bool is_touched;
    uint16_t baseline, filtered;
    int16_t delta;
    
    /* Sensor settings */

    // Thresholds (touch, release)
    // Default: 15, 10
    uint8_t tth = 15;
    uint8_t rth = 10;
    mpr121_set_thresholds(tth, rth, &mpr121);

    // Max half delta (rising, falling). Range 1~63
    // Default: 1, 1
    const uint8_t mhdr = 1;
    const uint8_t mhdf = 1;
    mpr121_set_max_half_delta(mhdr, mhdf, &mpr121);

    // Noise half delta (rising, falling, touched). Range 1~63
    // Peppe's sensor, 1, 1 ,3
    // Default: 1, 1, 1
    const uint8_t nhdr = 1;
    const uint8_t nhdf = 1;
    const uint8_t nhdt = 3;
    mpr121_set_noise_half_delta(nhdr, nhdf, nhdt, &mpr121);
    
    // Noise count limit (rising, falling, touched). Range 0~255.
    // Default: 0, 255, 0
    const uint8_t nclr = 0;
    const uint8_t nclf = 0;
    const uint8_t nclt = 0;
    mpr121_set_noise_count_limit(nclr, nclf, nclt, &mpr121);
    
    // Filter delay limit (rising, falling, touched). Range 0~255.
    // Default: 0, 2, 0
    const uint8_t fdlr = 0;
    const uint8_t fdlf = 0;
    const uint8_t fdlt = 0;
    mpr121_set_filter_delay_limit(fdlr, fdlf, fdlt, &mpr121);
    
    // Debounce (touch, release). Range 0~7.
    // Default: 0, 0
    const uint8_t tdbnc = 0;
    const uint8_t rdbnc = 0;
    mpr121_set_debounce(tdbnc, rdbnc, &mpr121);

    while(1) {
        // Check if the electrode has been touched, and read the
        // baseline and filtered data values (useful for debugging and
        // tuning the sensor).
        mpr121_baseline_value(electrode, &baseline, &mpr121);
        mpr121_filtered_data(electrode, &filtered, &mpr121);
        mpr121_is_touched(electrode, &is_touched, &mpr121);
        delta = baseline - filtered;

        // The on-board LED follows touch status.
        gpio_put(LED_PIN, is_touched);

        // Print the data.
        printf("%u %u %d %u %u %u\n", baseline, filtered, delta,
            tth, rth, is_touched * delta);

        // Pause.
        sleep_ms(50);
    }

    return 0;
}
