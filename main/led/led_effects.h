#pragma once
#include "util/color.h"

void led_init_ws2812(int gpio, int count);
void led_set_brightness(int brightness);
int led_get_brightness(void);
void led_set_effect_off(void);
void led_set_effect_solid(rgb_color_t color, int pixel);
void led_set_effect_blink(rgb_color_t color, int speed_ms, int pixel);
void led_set_effect_breathe(rgb_color_t color, int speed_ms);
const char* led_effect_name(void);
rgb_color_t led_current_color(void);