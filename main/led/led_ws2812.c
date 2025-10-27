#include "led_ws2812.h"
#include "led_strip.h"
#include "driver/rmt_tx.h"

static led_strip_handle_t strip;
static int s_count = 1;

static void fa(rgb_color_t c){ for(int i=0;i<s_count;i++) led_strip_set_pixel(strip, i, c.r, c.g, c.b); }
static void fo(int idx, rgb_color_t c){ if(idx>=0 && idx<s_count) led_strip_set_pixel(strip, idx, c.r, c.g, c.b); }
static void sh(void){ led_strip_refresh(strip); }

void _led_ws2812_bind(void (**fao)(rgb_color_t), void (**foo)(int,rgb_color_t), void (**sho)(void), int *count){
    *fao = fa; *foo = fo; *sho = sh; *count = s_count;
}

void _led_ws2812_hw_init(int gpio, int count){
    s_count = count;
    led_strip_config_t strip_cfg = {
        .strip_gpio_num = gpio,
        .max_leds = count,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &strip));
}