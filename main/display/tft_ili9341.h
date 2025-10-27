#pragma once 
#include <stdint.h> 

#define TFT_WIDTH   240   // 竖屏宽 
#define TFT_HEIGHT  320   // 竖屏高 

// 引脚（按你连法，如需改可在此处改） 
#define TFT_PIN_MOSI  11 
#define TFT_PIN_SCLK  12 
#define TFT_PIN_CS    10 
#define TFT_PIN_DC     9 
#define TFT_PIN_RST    8 
#define TFT_PIN_BLK   21   // 背光：高电平点亮（若直连3V3可忽略） 

void tft_init(void); 
void tft_fill_screen(uint16_t rgb565); 
void tft_fill_rect(int x, int y, int w, int h, uint16_t rgb565); 
void tft_draw_rect(int x, int y, int w, int h, uint16_t rgb565); 

// 5x7 数字/冒号 
void tft_draw_digit(int x, int y, int digit, int scale, uint16_t fg, uint16_t bg); 
void tft_draw_colon(int x, int y, int scale, uint16_t fg, uint16_t bg); 

// 5x7 ASCII（大写字母/数字/空格/冒号，足够画 "ID/STA/LEFT"） 
void tft_draw_text5x7(int x, int y, const char *txt, int scale, uint16_t fg, uint16_t bg); 

static inline uint16_t RGB565(uint8_t r,uint8_t g,uint8_t b){ 
    return ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3); 
}