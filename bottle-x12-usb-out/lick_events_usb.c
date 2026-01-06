/* Detect lick events and send via USB

When a lick takes place, the onset timestamp is printed to serial. The
master computer can read these values and save as required.

This is different from the script for e.g. optogenetics where lick
events are not detected; rather, the full on/off sensor signal is
redirected to a GPIO for further use. (That would need one extra
processing step to detect lick events.)
*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "pico/time.h"
#include "mpr121.h"

/* Touch sensor I2C definitions */
#define MPR121_I2C_PORT i2c0
#define MPR121_I2C_PIN_SDA 20
#define MPR121_I2C_PIN_SCL 21
#define MPR121_I2C_ADDRESS 0x5A
#define MPR121_I2C_FREQ 100000

/* Touch sensor settings definitions */
// #define SETTING_TTH 15
// #define SETTING_RTH 10
// #define SETTING_NHDR 1
// #define SETTING_NHDF 1
// #define SETTING_NHDT 3

/* Touch sensor variables */
uint16_t is_touched = 0;
uint16_t was_touched = 0;
uint16_t is_onset = 0;

/* Touch sensor structure */
struct mpr121_sensor mpr121;

/* Time variables */
absolute_time_t now;
uint32_t timestamp;

/* Repeating timer */
// Mice lick at up to ~10 Hz so this rate should be fine
const int32_t sampling_interval_ms = 20;  // 50 Hz
bool timer_callback(repeating_timer_t *rt);

/* Core 1 */
// void core1_entry() {
    // while (1) {
        // sleep_ms(20);
    // }
// }

uint16_t n = 0;

int main() {
    stdio_init_all();

    /* Setup the default, on-board LED */
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    /* Initialise I2C */
    i2c_init(MPR121_I2C_PORT, MPR121_I2C_FREQ);
    gpio_set_function(MPR121_I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(MPR121_I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(MPR121_I2C_PIN_SDA);
    gpio_pull_up(MPR121_I2C_PIN_SCL);

    /* Initialise the touch sensor */
    mpr121_init(MPR121_I2C_PORT, MPR121_I2C_ADDRESS, &mpr121);

    // How many electrodes to enable? If this value is 3, then
    // electrodes 0 to 2 will be enabled, etc. Max is thus 12 (enable
    // all electrodes, 0 to 11)
    mpr121_enable_electrodes(12, &mpr121);
    
    /* Initialise mutex and start core 1 */
    // mutex_init(&mtx);
    // multicore_launch_core1(core1_entry);

    /* Start repeating timer */
    repeating_timer_t timer;
    add_repeating_timer_ms(-sampling_interval_ms, timer_callback, NULL,
                           &timer);

    while(1) {
        tight_loop_contents();
    }
    return 0;
}


bool timer_callback(repeating_timer_t *rt) {
    // Read at once status of all electrodes. Bits 11-0 represent status
    // of each electrode
    mpr121_touched(&is_touched, &mpr121);

    // For testing, on-board LED follows status of electrode 0
    gpio_put(PICO_DEFAULT_LED_PIN, is_touched & 0b1);

    // Determine if there was a positive change in electrode status (it
    // was 0 but now it is 1). This indicates the onset of a touch event.
    //
    // E.g. at the onset of touch, electrode 1: was_touched is 0b00
    // is_touched is 0b10, is_onset is 0b10
    //
    // At the offset of touch, the same electrode: was_touched is 0b10,
    // is_touched is 0b00, is_onset is 0b00
    is_onset = (was_touched ^ is_touched) & is_touched;

    // If touch onset was detected in any electrode, print the timestamp
    // and electrode onset status
    if (is_onset){
        now = get_absolute_time();
        timestamp = to_ms_since_boot(now);
        printf("%d %d %d\n", n, timestamp, is_onset);
        n++;
    }    
    
    was_touched = is_touched;

    // if (needs_update && mutex_try_enter(&mtx, 0)) {
        // update_mpr121(settings, &curr_setting, &lcd, &mpr121);
        // needs_update = false;
        // mutex_exit(&mtx);
    // }
}
