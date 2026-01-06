#ifndef SETTINGS_H
#define SETTINGS_H

#include "pico.h"

// This value must match the number of parameters set by function
// `settings_init`
#define N_SETTINGS 7

/* 
mpr121_set_thresholds
mpr121_set_max_half_delta
mpr121_set_noise_half_delta
mpr121_set_noise_count_limit
*/
enum mpr121_func_key {
    FUNC_SET_TH,
    FUNC_SET_MHD,
    FUNC_SET_NHD,
    FUNC_SET_NCL
};

enum mpr121_setting_key {
    SETTING_TTH,
    SETTING_RTH,
    SETTING_MHDR,
    SETTING_MHDF,
    SETTING_NHDR,
    SETTING_NHDF,
    SETTING_NHDT
};

struct setting {
    uint8_t *name;
    uint8_t value;
    uint8_t min;
    uint8_t max;
    uint8_t step;
    enum mpr121_func_key func;
    enum mpr121_setting_key key;
};


static struct setting settings[N_SETTINGS];

extern void settings_init(struct setting *settings);

int8_t search_setting(enum mpr121_setting_key key,
                      struct setting *settings);

struct setting get_setting(enum mpr121_setting_key key,
                           struct setting *settings);

void setting_increase_value(struct setting *settings, uint8_t *idx);

void setting_decrease_value(struct setting *settings, uint8_t *idx);

#endif
