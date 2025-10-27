#pragma once
#include <stdint.h>

typedef struct {
    uint8_t r, g, b;
} rgb_color_t;

void hex_to_color(const char *hex, rgb_color_t *color);
void color_to_hex(rgb_color_t color, char *hex);