#include "led_effects.h"
#include "led_ws2812.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "LED_EFFECTS";

// LED控制函数指针
static void (*fill_all)(rgb_color_t) = NULL;
static void (*fill_one)(int, rgb_color_t) = NULL;
static void (*show)(void) = NULL;
static int led_count = 0;

// 当前状态
static rgb_color_t current_color = {0, 0, 0};
static int current_brightness = 255;
static const char *current_effect = "off";
static esp_timer_handle_t effect_timer = NULL;

// 效果状态
static int breathe_direction = 1;
static int breathe_value = 0;
static int blink_state = 0;

static void apply_brightness(rgb_color_t *color) {
    if (current_brightness < 255) {
        color->r = (color->r * current_brightness) / 255;
        color->g = (color->g * current_brightness) / 255;
        color->b = (color->b * current_brightness) / 255;
    }
}

static void effect_off(void) {
    rgb_color_t black = {0, 0, 0};
    if (fill_all) {
        fill_all(black);
        show();
    }
}

static void effect_solid(rgb_color_t color, int pixel) {
    apply_brightness(&color);
    if (pixel >= 0 && pixel < led_count) {
        if (fill_one) {
            fill_one(pixel, color);
        }
    } else {
        if (fill_all) {
            fill_all(color);
        }
    }
    if (show) show();
}

static void effect_blink(rgb_color_t color, int speed_ms, int pixel) {
    apply_brightness(&color);
    rgb_color_t black = {0, 0, 0};
    
    if (blink_state) {
        if (pixel >= 0 && pixel < led_count) {
            if (fill_one) fill_one(pixel, color);
        } else {
            if (fill_all) fill_all(color);
        }
    } else {
        if (pixel >= 0 && pixel < led_count) {
            if (fill_one) fill_one(pixel, black);
        } else {
            if (fill_all) fill_all(black);
        }
    }
    if (show) show();
    blink_state = !blink_state;
}

static void effect_breathe(rgb_color_t color, int speed_ms) {
    breathe_value += breathe_direction * 5;
    if (breathe_value >= 255) {
        breathe_value = 255;
        breathe_direction = -1;
    } else if (breathe_value <= 0) {
        breathe_value = 0;
        breathe_direction = 1;
    }
    
    rgb_color_t breathe_color = {
        (color.r * breathe_value) / 255,
        (color.g * breathe_value) / 255,
        (color.b * breathe_value) / 255
    };
    apply_brightness(&breathe_color);
    
    if (fill_all) {
        fill_all(breathe_color);
        show();
    }
}

static void effect_timer_callback(void* arg) {
    if (strcmp(current_effect, "blink") == 0) {
        effect_blink(current_color, 0, -1);
    } else if (strcmp(current_effect, "breathe") == 0) {
        effect_breathe(current_color, 0);
    }
}

void led_init_ws2812(int gpio, int count) {
    _led_ws2812_hw_init(gpio, count);
    _led_ws2812_bind(&fill_all, &fill_one, &show, &led_count);
    
    const esp_timer_create_args_t timer_args = {
        .callback = &effect_timer_callback,
        .name = "led_effect_timer"
    };
    esp_timer_create(&timer_args, &effect_timer);
    
    ESP_LOGI(TAG, "LED initialized: GPIO=%d, Count=%d", gpio, count);
}

void led_set_brightness(int brightness) {
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    current_brightness = brightness;
}

int led_get_brightness(void) {
    return current_brightness;
}

void led_set_effect_off(void) {
    if (effect_timer) esp_timer_stop(effect_timer);
    current_effect = "off";
    effect_off();
}

void led_set_effect_solid(rgb_color_t color, int pixel) {
    if (effect_timer) esp_timer_stop(effect_timer);
    current_effect = "solid";
    current_color = color;
    effect_solid(color, pixel);
}

void led_set_effect_blink(rgb_color_t color, int speed_ms, int pixel) {
    if (effect_timer) esp_timer_stop(effect_timer);
    current_effect = "blink";
    current_color = color;
    blink_state = 0;
    
    if (speed_ms < 100) speed_ms = 100;
    esp_timer_start_periodic(effect_timer, speed_ms * 1000);
}

void led_set_effect_breathe(rgb_color_t color, int speed_ms) {
    if (effect_timer) esp_timer_stop(effect_timer);
    current_effect = "breathe";
    current_color = color;
    breathe_value = 0;
    breathe_direction = 1;
    
    if (speed_ms < 100) speed_ms = 100;
    esp_timer_start_periodic(effect_timer, speed_ms * 100);
}

const char* led_effect_name(void) {
    return current_effect;
}

rgb_color_t led_current_color(void) {
    return current_color;
}