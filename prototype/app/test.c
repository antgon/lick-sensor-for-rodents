#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#include "mcp48x1.h"

// #include "settings.h"

/* DAC defines */
#define DAC_SPI_PORT spi0
#define DAC_SPI_BAUD 1 * 1000 * 1000 // 1 MHz. Max is 20 MHz
#define DAC_SPI_PIN_MOSI 3
#define DAC_SPI_PIN_CS 5
#define DAC_SPI_PIN_SCK 6

/* Repeating timer */
const int32_t sampling_interval_ms = 1;
bool timer_callback(repeating_timer_t *rt);

/*
Touch sensor values are 10 bit: 0-1023

`signal` (delta) is the difference between filtered and baseline, thus
in principle can be in the range -1023 to +1023.

I want to fit this range into 0 - 3.3 V

The DAC takes 12-bit uint: 0~4095 (0xfff)


V = VREF * G * (D/4096)

VREF = 2.048
D = digital value
G = gain

** D Max is 4095 **
* Gain 1x, output is 0 ~ 2.047 V
* Gain 2x, output is 0 ~ 4.095 V (if supply 5 V)
           output is 0 ~ 3.3 V (if supply 3V3)
           Useful codes 0 ~ 3299 (supply 3V3)

So this should work:
* Set gain to 2
* Useful range is 0 ~ 3299 (0 ~ 3.3 V)
* Add 1650 to signal, to center this about 1.65 V
* This gives the output range:
    * 1650 + 1023 = 2673 = 2.67 V
    * 1650 - 1023 = 627 = 0.627 V

*/

uint16_t dac_out = 0;
struct mcp48x1_dac dac;

int main() {
    stdio_init_all();

    bool dir_up = true;
    uint8_t step = 0;

    spi_init(DAC_SPI_PORT, DAC_SPI_BAUD);
    mcp48x1_init(DAC_SPI_PORT, DAC_SPI_PIN_CS, DAC_SPI_PIN_MOSI,
                 DAC_SPI_PIN_SCK, &dac);
    mcp48x1_set_gain(MCP48X1_GAIN_2X, &dac);

    /* Start repeating timer */
    repeating_timer_t timer;
    // add_repeating_timer_ms(-sampling_interval_ms, timer_callback,
                        //    NULL, &timer);
    add_repeating_timer_us(-50, timer_callback,
                           NULL, &timer);


    while(1) {
        tight_loop_contents();   
    }
    return 0;
}

bool timer_callback(repeating_timer_t *rt) {
         mcp48x1_put(dac_out, &dac);
        dac_out++;
        if (dac_out == 3300) {
            dac_out = 0;
        }
}
