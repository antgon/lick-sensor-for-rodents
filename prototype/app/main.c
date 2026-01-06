#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/sync.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"

#include "lcd16x2_i2c.h"
#include "mcp48x1.h"
#include "mpr121.h"

#include "settings.h"

/*
#include "pico/time.h"
absolute_time_t now;
absolute_time_t then;
int16_t tdiff;
*/

/* LCD defines */
#define LCD_I2C_PORT i2c0
#define LCD_I2C_PIN_SDA 12
#define LCD_I2C_PIN_SCL 13
#define LCD_I2C_ADDRESS 0x3E
#define LCD_I2C_FREQ 100000

/* DAC defines */
#define DAC_SPI_PORT spi0
#define DAC_SPI_BAUD 1 * 1000 * 1000 // 1 MHz
#define DAC_SPI_PIN_MOSI 3
#define DAC_SPI_PIN_CS 5
#define DAC_SPI_PIN_SCK 6

/* Touch sensor defines */
#define MPR121_I2C_PORT i2c1
#define MPR121_I2C_PIN_SDA 26
#define MPR121_I2C_PIN_SCL 27
#define MPR121_I2C_ADDRESS 0x5A
#define MPR121_I2C_FREQ 100000

/* Buttons defines */
#define BTN_UP 17
#define BTN_DOWN 18
#define BTN_LEFT 19
#define BTN_RIGHT 16

/* Digital out define */
#define TOUCH_OUT_PIN 11
const uint LED_PIN = PICO_DEFAULT_LED_PIN;

// ascii codes of useful characters
#define SPACE 32 
#define GREATER_THAN 62
#define EQUALS 61

/* LCD variables */
#define LCD_VALUE_COL 13
volatile bool needs_update = false;
uint8_t curr_setting = 0;

/* Touch sensor variables */
const uint8_t electrode = 0;
bool is_touched;
uint16_t baseline, filtered, delta_out;
int16_t delta;
// absolute_time_t now;

/* Repeating timer */
const int32_t sampling_interval_ms = 20;  // 50 Hz
bool timer_callback(repeating_timer_t *rt);

static mutex_t mtx;
struct mpr121_sensor mpr121;
struct lcd16x2 lcd;
struct mcp48x1_dac dac;

bool debounce_btn(uint8_t button){
    // From Listing 2 in www.ganssle.com/debouncing-pt2.htm
    static uint16_t state = 0;
    state = (state << 1) | !gpio_get(button) | 0xe000;
    if (state == 0xf000) return true;
    return false;
}

void lcd_put_number(uint8_t num, lcd16x2_t *lcd) {
    static uint8_t c[3];
    sprintf(c, "%3d", num);
    lcd_put_str(c, lcd);
}

void update_mpr121(struct setting *settings, uint8_t *idx,
                   lcd16x2_t *lcd, mpr121_sensor_t *mpr121) {
    switch (settings[*idx].func) {
        case FUNC_SET_TH:
            struct setting tth, rth;
            tth = get_setting(SETTING_TTH, settings);
            rth = get_setting(SETTING_RTH, settings);
            mpr121_set_thresholds(tth.value, rth.value, mpr121);
            break;
        case FUNC_SET_NHD:
            struct setting nhdr, nhdf, nhdt;
            nhdr = get_setting(SETTING_NHDR, settings);
            nhdf = get_setting(SETTING_NHDF, settings);
            nhdt = get_setting(SETTING_NHDT, settings);
            mpr121_set_noise_half_delta(nhdr.value, nhdf.value,
                                        nhdt.value, mpr121);
            break;
        case FUNC_SET_MHD:
            struct setting mhdr, mhdf;
            mhdr = get_setting(SETTING_MHDR, settings);
            mhdf = get_setting(SETTING_MHDF, settings);         
            mpr121_set_max_half_delta(mhdr.value, mhdf.value, mpr121);
            break;
    }
    // Update LCD with new value
    lcd_move_cursor(0, LCD_VALUE_COL, lcd);
    lcd_put_number(settings[*idx].value, lcd);
}

void lcd_put_setting(struct setting *settings, uint8_t *idx,
                     lcd16x2_t *lcd) {
    lcd_move_cursor(0, 0, lcd);
    lcd_put_str(settings[*idx].name, lcd);
    lcd_move_cursor(0, LCD_VALUE_COL, lcd);
    lcd_put_number(settings[*idx].value, lcd);
}

/* Core 1: Handle LCD and buttons */
void core1_entry() {
    /* Initialise buttons */
    gpio_init(BTN_UP);
    gpio_set_dir(BTN_UP, GPIO_IN);
    gpio_pull_up(BTN_UP);
    gpio_init(BTN_DOWN);
    gpio_set_dir(BTN_DOWN, GPIO_IN);
    gpio_pull_up(BTN_DOWN);
    gpio_init(BTN_LEFT);
    gpio_set_dir(BTN_LEFT, GPIO_IN);
    gpio_pull_up(BTN_LEFT);
    gpio_init(BTN_RIGHT);
    gpio_set_dir(BTN_RIGHT, GPIO_IN);
    gpio_pull_up(BTN_RIGHT);

    /* Initialise I2C for LCD */
    i2c_init(LCD_I2C_PORT, LCD_I2C_FREQ);
    gpio_set_function(LCD_I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(LCD_I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(LCD_I2C_PIN_SDA);
    gpio_pull_up(LCD_I2C_PIN_SCL);
    
    /* Initialise LCD */
    lcd_init(LCD_I2C_PORT, LCD_I2C_ADDRESS, &lcd);
    lcd_put_setting(settings, &curr_setting, &lcd);
    lcd_move_cursor(0, LCD_VALUE_COL-1, &lcd);
    lcd_put_char(EQUALS, &lcd);

    /* Poll buttons */
    while (1) {
        /* Button down: next setting */
        if (debounce_btn(BTN_DOWN)) {
            curr_setting++;                
            if (curr_setting == N_SETTINGS) {
                // If already at last setting, just keep the current
                // index at that place and do nothing.
                curr_setting = (N_SETTINGS - 1);
            } else {
                lcd_put_setting(settings, &curr_setting, &lcd);
            }
        }
        /* Button up: previous setting */
        if (debounce_btn(BTN_UP)) {
            curr_setting--;
            // If curr_setting goes below 0 it will overflow back to
            // some large-ish number (because it is uint), which means
            // that we already are at the first index. If this happens
            // keep the value at 0.
            if (curr_setting > N_SETTINGS) {
                curr_setting = 0;
            } else {
                lcd_put_setting(settings, &curr_setting, &lcd);
            }            
        }
        /* Button right: increase the value of the current setting */
        if (debounce_btn(BTN_RIGHT)) {
            mutex_enter_blocking(&mtx);
            setting_increase_value(settings, &curr_setting);
            needs_update = true;
            mutex_exit(&mtx);
        }
        /* Button left: decrease the value of the current setting */
        if (debounce_btn(BTN_LEFT)) {
            mutex_enter_blocking(&mtx);
            setting_decrease_value(settings, &curr_setting);
            needs_update = true;
            mutex_exit(&mtx);
        }
        sleep_ms(20);
    }
}

int main() {
    stdio_init_all();

    /* Digital output pin */
    gpio_init(TOUCH_OUT_PIN);
    gpio_set_dir(TOUCH_OUT_PIN, GPIO_OUT);

    /* Setup the default, on-board LED */    
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    /* Initialise I2C for touch sensor */
    i2c_init(MPR121_I2C_PORT, MPR121_I2C_FREQ);
    gpio_set_function(MPR121_I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(MPR121_I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(MPR121_I2C_PIN_SDA);
    gpio_pull_up(MPR121_I2C_PIN_SCL);

    /* Initialise the touch sensor. Enable only one touch sensor
    (electrode 0) */
    mpr121_init(MPR121_I2C_PORT, MPR121_I2C_ADDRESS, &mpr121);
    mpr121_enable_electrodes(1, &mpr121);
    settings_init(settings);
    struct setting tth = get_setting(SETTING_TTH, settings);
    struct setting rth = get_setting(SETTING_RTH, settings);
    mpr121_set_thresholds(tth.value, rth.value, &mpr121);
    struct setting nhdr = get_setting(SETTING_NHDR, settings);
    struct setting nhdf = get_setting(SETTING_NHDF, settings);
    struct setting nhdt = get_setting(SETTING_NHDT, settings);
    mpr121_set_noise_half_delta(nhdr.value, nhdf.value, nhdt.value,
                                &mpr121);
    // mpr121_set_max_half_delta(mhdr, mhdf, &mpr121);
    // mpr121_set_noise_count_limit(nclr, nclf, nclt, &mpr121);

    /* Initialise SPI and DAC */
    spi_init(DAC_SPI_PORT, DAC_SPI_BAUD);
    mcp48x1_init(DAC_SPI_PORT, DAC_SPI_PIN_CS, DAC_SPI_PIN_MOSI,
                 DAC_SPI_PIN_SCK, MCP48X1_RESOLUTION_12, &dac);
    mcp48x1_set_gain(MCP48X1_GAIN_2X, &dac);

    /* Initialise mutex and start core 1 */
    mutex_init(&mtx);
    multicore_launch_core1(core1_entry);

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
    // now = get_absolute_time();
    // tdiff = to_ms_since_boot(now) - to_ms_since_boot(then);
    // then = now;

    // Check if the electrode has been touched, and read the
    // baseline and filtered data values
    mpr121_is_touched(electrode, &is_touched, &mpr121);
    mpr121_baseline_value(electrode, &baseline, &mpr121);
    mpr121_filtered_data(electrode, &filtered, &mpr121);
    delta = baseline - filtered;

    // DAC gain is set to 2x. Thus, its useful range is 0 ~ 3299 (0 ~
    // 3.3 V). Both baseline and filtered are 10-bit numbers, which
    // means that delta can be a number between -1023 and +1023.
    //
    // To output this delta value to the DAC, which is 12-bit, I add
    // 1650 to center delta at 1.65 V. This gives an output range for
    // delta of:
    //   * 1650 + 1023 = 2673 = 2.67 V
    //   * 1650 - 1023 = 627 = 0.627 V
    //
    // Multiplying delta by 1.5 extends the useful output range
    //   * 1650 + (+1023 * 1.5) = 3185 = 3.19 V
    //   * 1650 + (-1023 * 1.5) = 116 = 0.12 V
    delta_out = (uint16_t)(1650.0 + (delta * 1.5));
    
    // Output: is_touched (digital) and delta (analog) values
    gpio_put(TOUCH_OUT_PIN, is_touched);
    gpio_put(LED_PIN, is_touched);
    mcp48x1_put(delta_out, &dac);
    
    // printf("%d %d %d %d %d %d\n", baseline, filtered, delta,     
    //     settings[0].value, settings[1].value,
    //     // delta_out , mhdf,
    //     is_touched);

    // printf("%d\n", tdiff);
    
    if (needs_update && mutex_try_enter(&mtx, 0)) {
        update_mpr121(settings, &curr_setting, &lcd, &mpr121);
        needs_update = false;
        mutex_exit(&mtx);
    }
}