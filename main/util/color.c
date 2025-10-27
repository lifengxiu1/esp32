#include "color.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void hex_to_color(const char *hex, rgb_color_t *color) {
    if (!hex || !color) return;
    
    // 跳过#号
    if (*hex == '#') hex++;
    
    // 解析十六进制颜色
    unsigned int r, g, b;
    if (strlen(hex) >= 6) {
        sscanf(hex, "%2x%2x%2x", &r, &g, &b);
        color->r = (uint8_t)r;
        color->g = (uint8_t)g;
        color->b = (uint8_t)b;
    } else {
        // 默认颜色
        color->r = 0xFF;
        color->g = 0xFF;
        color->b = 0xFF;
    }
}

void color_to_hex(rgb_color_t color, char *hex) {
    if (!hex) return;
    sprintf(hex, "#%02X%02X%02X", color.r, color.g, color.b);
}