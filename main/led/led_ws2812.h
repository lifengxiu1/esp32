#pragma once
#include "util/color.h"
void _led_ws2812_hw_init(int gpio, int count);
void _led_ws2812_bind(void (**fa)(rgb_color_t), void (**fo)(int,rgb_color_t), void (**sh)(void), int *count);