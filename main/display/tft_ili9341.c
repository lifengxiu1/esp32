#include "tft_ili9341.h" 
#include "driver/spi_master.h" 
#include "driver/gpio.h" 
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h" 
#include <string.h> 

static spi_device_handle_t s_dev; 

static inline void gpio_out(int pin, int level){ 
    if (pin>=0) gpio_set_level(pin, level); 
} 

static void send_cmd(uint8_t cmd){ 
    gpio_out(TFT_PIN_DC, 0); 
    spi_transaction_t t = { .length = 8, .tx_buffer = &cmd }; 
    spi_device_transmit(s_dev, &t); 
} 

static void send_data(const void *data, int len){ 
    if (len<=0) return; 
    gpio_out(TFT_PIN_DC, 1); 
    spi_transaction_t t = { .length = 8*len, .tx_buffer = data }; 
    spi_device_transmit(s_dev, &t); 
} 

static void set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1){ 
    uint8_t buf[4]; 
    send_cmd(0x2A); // Column 
    buf[0]=x0>>8; buf[1]=x0&0xFF; buf[2]=x1>>8; buf[3]=x1&0xFF; send_data(buf,4); 
    send_cmd(0x2B); // Page 
    buf[0]=y0>>8; buf[1]=y0&0xFF; buf[2]=y1>>8; buf[3]=y1&0xFF; send_data(buf,4); 
    send_cmd(0x2C); // Memory write 
} 

void tft_init(void){ 
    // DC/RST/BLK 
    gpio_config_t io = { 
        .mode = GPIO_MODE_OUTPUT, 
        .pin_bit_mask = (1ULL<<TFT_PIN_DC) | (1ULL<<TFT_PIN_RST) | (1ULL<<TFT_PIN_BLK) 
    }; 
    gpio_config(&io); 
    gpio_out(TFT_PIN_BLK, 1); // 背光 ON 

    // SPI2 
    spi_bus_config_t bus = { 
        .mosi_io_num = TFT_PIN_MOSI, 
        .miso_io_num = -1, 
        .sclk_io_num = TFT_PIN_SCLK, 
        .quadwp_io_num = -1, 
        .quadhd_io_num = -1, 
        .max_transfer_sz = TFT_WIDTH*2 + 8, 
    }; 
    spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO); 

    spi_device_interface_config_t dev = { 
        .clock_speed_hz = 26*1000*1000, // 26MHz 稳妥 
        .mode = 0, .spics_io_num = TFT_PIN_CS, .queue_size = 7, 
        .flags = SPI_DEVICE_HALFDUPLEX 
    }; 
    spi_bus_add_device(SPI2_HOST, &dev, &s_dev); 

    // 复位 
    gpio_out(TFT_PIN_RST, 0); vTaskDelay(pdMS_TO_TICKS(20)); 
    gpio_out(TFT_PIN_RST, 1); vTaskDelay(pdMS_TO_TICKS(120)); 

    // 初始化序列（精简） 
    send_cmd(0x01); vTaskDelay(pdMS_TO_TICKS(5));     // SWRESET 
    send_cmd(0x11); vTaskDelay(pdMS_TO_TICKS(120));   // SLPOUT 
    uint8_t pixfmt = 0x55;                             // 16bit 
    send_cmd(0x3A); send_data(&pixfmt,1); 

    // 竖屏方向；如颜色异常/颠倒可在 0x48/0x88/0x28/0x68 里试 
    uint8_t mad = 0x48; // MY + BGR 
    send_cmd(0x36); send_data(&mad,1); 

    send_cmd(0x29); // DISPON 
    tft_fill_screen(RGB565(255,255,255)); 
} 

void tft_fill_screen(uint16_t rgb565){ 
    tft_fill_rect(0,0,TFT_WIDTH,TFT_HEIGHT,rgb565); 
}

void tft_fill_rect(int x, int y, int w, int h, uint16_t c){ 
    if (x<0){w+=x; x=0;} if (y<0){h+=y; y=0;} 
    if (x>=TFT_WIDTH || y>=TFT_HEIGHT) return; 
    if (x+w > TFT_WIDTH ) w = TFT_WIDTH  - x; 
    if (y+h > TFT_HEIGHT) h = TFT_HEIGHT - y; 
    if (w<=0 || h<=0) return; 

    set_addr_window(x,y,x+w-1,y+h-1); 

    uint16_t line[240]; 
    int maxn = (w>240?240:w); 
    for(int i=0;i<maxn;i++) line[i]=c; 

    for(int row=0; row<h; ++row){ 
        int remain = w; 
        while(remain>0){ 
            int n = remain>maxn ? maxn : remain; 
            send_data(line, n*2); 
            remain -= n; 
        } 
    } 
} 

void tft_draw_rect(int x, int y, int w, int h, uint16_t c){ 
    tft_fill_rect(x,y,w,1,c); 
    tft_fill_rect(x,y+h-1,w,1,c); 
    tft_fill_rect(x,y,1,h,c); 
    tft_fill_rect(x+w-1,y,1,h,c); 
} 

/* ---- 5x7 数字/冒号 ---- */ 
static const uint8_t font5x7_digits[11][5] = { 
    {0x3E,0x51,0x49,0x45,0x3E}, // 0 
    {0x00,0x42,0x7F,0x40,0x00}, // 1 
    {0x42,0x61,0x51,0x49,0x46}, // 2 
    {0x21,0x41,0x45,0x4B,0x31}, // 3 
    {0x18,0x14,0x12,0x7F,0x10}, // 4 
    {0x27,0x45,0x45,0x45,0x39}, // 5 
    {0x3C,0x4A,0x49,0x49,0x30}, // 6 
    {0x01,0x71,0x09,0x05,0x03}, // 7 
    {0x36,0x49,0x49,0x49,0x36}, // 8 
    {0x06,0x49,0x49,0x29,0x1E}, // 9 
    {0x00,0x36,0x36,0x00,0x00}, // : 
}; 

static void draw_glyph5x7(int x,int y,const uint8_t glyph[5],int scale,uint16_t fg,uint16_t bg){ 
    for(int col=0; col<5; ++col){ 
        uint8_t colbits = glyph[col]; 
        for(int row=0; row<7; ++row){ 
            int on = (colbits >> row) & 1; 
            if(on){ 
                tft_fill_rect(x+col*scale, y+row*scale, scale, scale, fg); 
            } else if (bg != 0xFFFF){ 
                tft_fill_rect(x+col*scale, y+row*scale, scale, scale, bg); 
            } 
        } 
    } 
} 

void tft_draw_digit(int x,int y,int digit,int scale,uint16_t fg,uint16_t bg){ 
    if (digit<0 || digit>9) return; 
    draw_glyph5x7(x,y,font5x7_digits[digit],scale,fg,bg); 
} 
void tft_draw_colon(int x,int y,int scale,uint16_t fg,uint16_t bg){ 
    draw_glyph5x7(x,y,font5x7_digits[10],scale,fg,bg); 
} 

/* ---- 5x7 ASCII（空格/冒号/部分大写字母 + 数字） ---- */ 
static const uint8_t* glyph_of_ascii(char ch){ 
    static const uint8_t SPACE[5]={0,0,0,0,0}; 
    static const uint8_t COLON[5]={0x00,0x36,0x36,0x00,0x00}; 
    if (ch>='0' && ch<='9') return font5x7_digits[ch-'0']; 
    switch(ch){ 
        case ' ': return SPACE; case ':': return COLON; 
        case 'A': {static const uint8_t a[5]={0x7E,0x11,0x11,0x11,0x7E}; return a;} 
        case 'D': {static const uint8_t d[5]={0x7F,0x41,0x41,0x22,0x1C}; return d;} 
        case 'E': {static const uint8_t e[5]={0x7F,0x49,0x49,0x49,0x41}; return e;} 
        case 'F': {static const uint8_t f[5]={0x7F,0x09,0x09,0x09,0x01}; return f;} 
        case 'I': {static const uint8_t i[5]={0x00,0x41,0x7F,0x41,0x00}; return i;} 
        case 'L': {static const uint8_t l[5]={0x7F,0x40,0x40,0x40,0x40}; return l;} 
        case 'S': {static const uint8_t s[5]={0x46,0x49,0x49,0x49,0x31}; return s;} 
        case 'T': {static const uint8_t t[5]={0x01,0x01,0x7F,0x01,0x01}; return t;} 
        default:  return SPACE; 
    } 
} 

void tft_draw_text5x7(int x,int y,const char *txt,int scale,uint16_t fg,uint16_t bg){ 
    for(const char *p=txt; *p; ++p){ 
        char ch = *p; if (ch>='a' && ch<='z') ch -= 32; // 转大写 
        const uint8_t *glyph = glyph_of_ascii(ch); 
        draw_glyph5x7(x,y,glyph,scale,fg,bg); 
        x += 6*scale; // 5列 + 1列间隔 
    } 
}