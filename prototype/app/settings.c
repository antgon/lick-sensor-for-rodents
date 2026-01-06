#include "settings.h"


void settings_init(struct setting *settings) {
    // Name must be 13 characters long

    /* Jitter
       Touch and release thresholds, range 0~255, default 15/10 */
    uint8_t idx = 0;
    settings[idx].name = "Touch thres ";
    settings[idx].value = 15;
    settings[idx].min = 0;
    settings[idx].max = 255;
    settings[idx].step = 2;
    settings[idx].func = FUNC_SET_TH;
    settings[idx].key = SETTING_TTH;

    idx = 1;
    settings[idx].name = "Release th  ";
    settings[idx].value = 10;
    settings[idx].min = 0;
    settings[idx].max = 255;
    settings[idx].step = 2;
    settings[idx].func = FUNC_SET_TH;
    settings[idx].key = SETTING_RTH;

    /* Baseline system
       Noise half delta rising/falling/touched, range 1~63, default 1 */
    idx = 2;
    settings[idx].name = "NHD rising  ";
    settings[idx].value = 1;
    settings[idx].min = 1;
    settings[idx].max = 63;
    settings[idx].step = 1;
    settings[idx].func = FUNC_SET_NHD;
    settings[idx].key = SETTING_NHDR;

    idx = 3;
    settings[idx].name = "NHD falling ";
    settings[idx].value = 1;
    settings[idx].min = 1;
    settings[idx].max = 63;
    settings[idx].step = 1;
    settings[idx].func = FUNC_SET_NHD;
    settings[idx].key = SETTING_NHDF;

    idx = 4;
    settings[idx].name = "NHD touched ";
    settings[idx].value = 3;
    settings[idx].min = 1;
    settings[idx].max = 63;
    settings[idx].step = 1;
    settings[idx].func = FUNC_SET_NHD;
    settings[idx].key = SETTING_NHDT;

    /* Baseline system
       Max half delta rising/falling, range 1~63, default 1 */
    idx = 5;
    settings[idx].name = "MHD rising  ";
    settings[idx].value = 1;
    settings[idx].min = 1;
    settings[idx].max = 63;
    settings[idx].step = 1;
    settings[idx].func = FUNC_SET_MHD;
    settings[idx].key = SETTING_MHDR;

    idx = 6;
    settings[idx].name = "MHD falling ";
    settings[idx].value = 1;
    settings[idx].min = 1;
    settings[idx].max = 63;
    settings[idx].step = 1;
    settings[idx].func = FUNC_SET_MHD;
    settings[idx].key = SETTING_MHDF;
};


int8_t search_setting(enum mpr121_setting_key key,
                      struct setting *settings) {
    for (uint8_t i = 0; i < N_SETTINGS; i++) {
        if (settings[i].key == key) return i;
    }
    return -1;
}


struct setting get_setting(enum mpr121_setting_key key,
                           struct setting *settings) {
    int8_t ret = search_setting(key, settings);
    return settings[ret];    
}

void setting_increase_value(struct setting *settings, uint8_t *idx) {
    // Needs a longer int to be able to determine if the new value is
    // greater than 255, which is the max in some parameters.
    int16_t val = (int16_t)(settings[*idx].value) + settings[*idx].step;
    if (val > settings[*idx].max) {
        settings[*idx].value = settings[*idx].max;
    } else {
        settings[*idx].value = (uint8_t)val;
    }
}

void setting_decrease_value(struct setting *settings, uint8_t *idx) {
    int16_t val = (int16_t)(settings[*idx].value) - settings[*idx].step;
    if (val < settings[*idx].min) {
        settings[*idx].value = settings[*idx].min;
    } else {
        settings[*idx].value = (uint8_t)val;
    }
}