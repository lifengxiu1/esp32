#include "status_list.h" 
#include "tft_ili9341.h" 
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h" 
#include "freertos/semphr.h" 
#include <string.h> 
#include <stdlib.h>

// ===== 主题配色：白底黑字，方块 红=亮 / 白=灭 ===== 
#define COL_BG              RGB565(255,255,255)  // 整页与清屏底色 
#define COL_TEXT            RGB565(0,0,0)        // 编号/标题/正文 
#define COL_TIME            RGB565(0,0,0)        // mm:ss 
#define COL_STATUS_ON       RGB565(255,0,0)      // 亮：红 
#define COL_STATUS_OFF      RGB565(255,255,255)  // 灭：白（和背景同色） 
#define COL_BORDER          RGB565(0,0,0)        // 方块/分隔线/页码条 
#define COL_PAGE_ACTIVE     RGB565(0,0,0)        // 当前页条 
#define COL_PAGE_INACTIVE   RGB565(160,160,160)  // 非当前页条

#define SNAP_INVALID 0xFF   // d_on[] 中的"未绘制"标记

// 回调指针
static status_led_sync_cb_t s_led_sync_cb = NULL;

static int s_total = 0; 
static uint8_t  *s_on = NULL;          // 实际状态 
static uint32_t *s_remain_ms = NULL;   // 实际剩余毫秒(0=不计时) 
static SemaphoreHandle_t s_lock; 

// 为了减少闪烁：记录"当前已绘制快照"，只改动变化的部分 
static uint8_t  *d_on = NULL;          // 已绘制状态 
static uint32_t *d_remain_s = NULL;    // 已绘制的剩余秒数 

// ------- 布局（竖屏，两列，紧凑） ------- 
static const int kHeaderH   = 28;   // 英文表头高度 
static const int kLineH     = 26;   // 行高（紧凑） 
static const int kColGap    = 4;    // 列间距 
static const int kMargin    = 4;    // 边距 
static const int kBoxSize   = 10;   // 状态小方块 
static const int kNumScale  = 2;    // 编号字号（10x14） 
static const int kTimeScale = 3;    // 时间字号（更大） 
static const int kGapM      = 4;    // 中间距 

static int rows_per_col(void){ 
    int usable = TFT_HEIGHT - kHeaderH - 6; 
    int r = usable / kLineH; 
    return (r<10)?10:r; // 一般能到 12 行/列 
} 
static int col_width(void){ 
    return (TFT_WIDTH - 2*kMargin - kColGap) / 2; // ~114 px 
} 
static int per_page(void){ return rows_per_col() * 2; } 
static int pages_count(void){ 
    int pp = per_page(); 
    int p = (s_total + pp - 1)/pp; 
    return p<1?1:p; 
} 

// 两位编号（01..），相邻数字步进=5*scale（紧凑） 
static void draw_id2_compact(int x, int y, int id1, int scale, uint16_t fg){ 
    int tens = (id1/10)%10, ones = id1%10; 
    tft_draw_digit(x,              y, tens, scale, fg, 0xFFFF); 
    tft_draw_digit(x + 5*scale,    y, ones, scale, fg, 0xFFFF); 
} 

// mm:ss（紧凑步进；冒号 5 列等宽） 
static void draw_mmss_compact(int x, int y, uint32_t ms, int scale, uint16_t fg){ 
    uint32_t s = ms/1000; int mm = s/60, ss = s%60; 
    int d0=(mm/10)%10, d1=mm%10, d2=(ss/10)%10, d3=ss%10; 
    tft_draw_digit(x,               y, d0, scale, fg, 0xFFFF); x += 5*scale; 
    tft_draw_digit(x,               y, d1, scale, fg, 0xFFFF); x += 5*scale; 
    tft_draw_colon(x,               y,      scale, fg, 0xFFFF); x += 5*scale; 
    tft_draw_digit(x,               y, d2, scale, fg, 0xFFFF); x += 5*scale; 
    tft_draw_digit(x,               y, d3, scale, fg, 0xFFFF); 
} 

// ------- 表头（白底黑字：ID｜S｜T，S/T 水平居中到栏位） ------- 
static void draw_header_for_column(int x0){ 
    int w = col_width(); 
    tft_fill_rect(x0, 0, w, kHeaderH, COL_BG);  // 白底 

    // 与行内对齐的三个起点（同 draw_row） 
    int x_num = x0 + 2;                                      // ID 起点（左对齐） 
    int x_sta = x_num + (10*kNumScale) + kGapM;              // 紧随两位编号 
    int x_tim = x_sta + kBoxSize + kGapM;                    // 紧随状态块 

    // 文本垂直居中：5x7 @ scale=2 的高度为 14 像素 
    int y_text = (kHeaderH - 14)/2; 

    // 单个字符在 scale=2 下的"字形宽度"（不含字间隔） 
    const int char_w_s2 = 5 * 2; 

    // 时间栏可用宽度（用于 mm:ss 的紧凑宽度）：5 glyph * (5 * kTimeScale) 
    int time_w = (5 * kTimeScale) * 5; 

    // 让 S 居中在状态块宽度内，让 T 居中在时间栏宽度内 
    int x_s_title = x_sta + (kBoxSize - char_w_s2)/2; 
    int x_t_title = x_tim + (time_w  - char_w_s2)/2; 
    if (x_s_title < x_sta) x_s_title = x_sta;  // 防御性 
    if (x_t_title < x_tim) x_t_title = x_tim; 

    // 画标题 
    tft_draw_text5x7(x_num,    y_text, "ID", 2, COL_TEXT, 0xFFFF); 
    tft_draw_text5x7(x_s_title,y_text, "S",  2, COL_TEXT, 0xFFFF); 
    tft_draw_text5x7(x_t_title,y_text, "T",  2, COL_TEXT, 0xFFFF); 

    // 底部分隔线 
    tft_fill_rect(x0, kHeaderH-1, w, 1, COL_BORDER); 
} 

// ------- 绘制"当前页中的第 row_in_page 条"（增量） ------- 
static void draw_row(int page, int row_in_page){ 
    int rpc = rows_per_col(); 
    int idx = page*per_page() + row_in_page; 
    if (idx >= s_total) return; 

    int col = (row_in_page < rpc) ? 0 : 1; 
    int row = (row_in_page < rpc) ? row_in_page : (row_in_page - rpc); 

    int colW  = col_width(); 
    int baseX = (col==0) ? (kMargin) : (kMargin + colW + kColGap); 
    int y     = kHeaderH + 4 + row * kLineH; 

    int x_num = baseX + 2; 
    int x_sta = x_num + (10*kNumScale) + kGapM; 
    int x_tim = x_sta + kBoxSize + kGapM; 
    int time_w = (5 * kTimeScale) * 5;   // "mm:ss" 紧凑总宽 = 5 glyph * (5*scale) 

    // 读取当前状态与旧快照 
    xSemaphoreTake(s_lock, portMAX_DELAY); 
    int on = s_on[idx] ? 1 : 0; 
    uint32_t ms = s_remain_ms[idx]; 
    int on_old  = d_on ? d_on[idx] : SNAP_INVALID;       // 原来是 -1 
    uint32_t sec_old = d_remain_s ? d_remain_s[idx] : 0xFFFFFFFF; 
    xSemaphoreGive(s_lock); 

    // 初次：清一次整行背景并画编号 
    if (on_old == SNAP_INVALID) { 
        // 首次进入该行：清整行并画编号 
        tft_fill_rect(baseX, y-1, colW, kLineH-1, COL_BG); 
        draw_id2_compact(x_num, y, idx+1, kNumScale, COL_TEXT); 
    } 

    // 状态块变化才重画 
    if (on != on_old){ 
        uint16_t fill = on ? COL_STATUS_ON : COL_STATUS_OFF; // 红 / 白 
        tft_fill_rect(x_sta, y, kBoxSize, kBoxSize, fill); 
        tft_draw_rect(x_sta, y, kBoxSize, kBoxSize, COL_BORDER); // 黑色边框 
        // 若灭，清时间区为白 
        if (!on){ 
            tft_fill_rect(x_tim, y, time_w, 14*kTimeScale, COL_BG); 
        } 
    } 

    // 时间区：仅当"亮且秒数变化"时重画 
    uint32_t sec_now = on ? (ms/1000) : 0; 
    if (on && sec_now != sec_old){ 
        // 仅清时间区为白，再画黑字时间 
        tft_fill_rect(x_tim, y, time_w, 14*kTimeScale, COL_BG); 
        draw_mmss_compact(x_tim, y, ms, kTimeScale, COL_TIME); 
    }else if (!on && on_old == SNAP_INVALID){ 
        tft_fill_rect(x_tim, y, time_w, 14*kTimeScale, COL_BG); 
    } 

    // 更新快照 
    if (d_on && d_remain_s){ 
        d_on[idx] = on; 
        d_remain_s[idx] = sec_now; 
    } 
} 

static void draw_page_full(int page){ 
    tft_fill_screen(COL_BG); 

    int colW = col_width(); 
    int leftX  = kMargin; 
    int rightX = kMargin + colW + kColGap; 
    draw_header_for_column(leftX); 
    draw_header_for_column(rightX); 

    int pp = per_page(); 
    for(int r=0; r<pp; ++r){ 
        int idx = page*pp + r; 
        if (idx >= s_total) break; 
        // 标记未绘制，便于 draw_row 完整画一次 
        if (d_on)       d_on[idx]       = SNAP_INVALID; 
        if (d_remain_s) d_remain_s[idx] = 0xFFFFFFFF; 
        draw_row(page, r); 
    } 

    // 页码条 
    int pages = pages_count(); 
    if (pages>1){ 
        int bar_y = TFT_HEIGHT-3; 
        int bar_w = (TFT_WIDTH-16)/pages; 
        for(int p=0;p<pages;p++){ 
            uint16_t col = (p==page)? COL_PAGE_ACTIVE : COL_PAGE_INACTIVE; 
            int bx = 8 + p*bar_w; 
            tft_fill_rect(bx, bar_y, bar_w-2, 2, col); 
        } 
    } 
} 

// --------- UI 任务：每秒只增量刷新当前页，5秒整页翻页 ---------- 
static void ui_task(void *){ 
    const TickType_t T1S = pdMS_TO_TICKS(1000); 
    const TickType_t T5S = pdMS_TO_TICKS(5000); 
    TickType_t last_tick = xTaskGetTickCount(); 
    TickType_t last_page = last_tick; 
    int page = 0; 

    draw_page_full(page); 

    for(;;){ 
        vTaskDelay(pdMS_TO_TICKS(30)); 
        TickType_t now = xTaskGetTickCount(); 

        // === ui_task 内，每秒逻辑 ===
        if (now - last_tick >= T1S){
            last_tick += T1S;

            // 1) 先倒计时 & 记录刚从亮 -> 灭 的索引
            int expired_idx[64];     // 每秒最多同时到期的数量（足够用；可按需加大）
            int expired_cnt = 0;

            xSemaphoreTake(s_lock, portMAX_DELAY);
            for(int i=0;i<s_total;i++){
                if (s_on[i] && s_remain_ms[i] > 0){
                    if (s_remain_ms[i] > 1000) s_remain_ms[i] -= 1000;
                    else s_remain_ms[i] = 0;

                    if (s_remain_ms[i] == 0){
                        s_on[i] = 0;  // 显示状态改为"灭"
                        if (expired_cnt < (int)(sizeof(expired_idx)/sizeof(expired_idx[0]))){
                            expired_idx[expired_cnt++] = i;  // 记录这个刚到期的像素
                        }
                    }
                }
            }
            xSemaphoreGive(s_lock);

            // 2) 再在锁外逐个回调：同步实际 LED 硬件为"灭"
            if (s_led_sync_cb){
                for(int k=0; k<expired_cnt; ++k){
                    s_led_sync_cb(expired_idx[k], 0 /*off*/);
                }
            }

            // 3) 增量刷新当前页（原有逻辑保留）
            int pp  = per_page();
            for(int r=0; r<pp; ++r){
                int idx = page*pp + r;
                if (idx >= s_total) break;

                xSemaphoreTake(s_lock, portMAX_DELAY);
                int on_now = s_on[idx];
                uint32_t sec_now = s_remain_ms[idx]/1000;
                int on_old2 = d_on ? d_on[idx] : SNAP_INVALID;
                uint32_t sec_old2 = d_remain_s ? d_remain_s[idx] : 0xFFFFFFFF;
                xSemaphoreGive(s_lock);

                if (on_now != on_old2 || sec_now != sec_old2){
                    draw_row(page, r);
                }
            }
        } 

        // 每5秒翻页 
        if (pages_count()>1 && now - last_page >= T5S){ 
            last_page += T5S; 
            page = (page+1) % pages_count(); 
            draw_page_full(page); 
        } 
    } 
} 

// --------- 对外接口 ---------- 
void status_list_init(int total_leds){ 
    s_total = total_leds; 
    s_on = (uint8_t*)calloc(s_total, 1); 
    s_remain_ms = (uint32_t*)calloc(s_total, sizeof(uint32_t)); 
    d_on = (uint8_t*)calloc(s_total, 1); 
    d_remain_s = (uint32_t*)calloc(s_total, sizeof(uint32_t)); 
    s_lock = xSemaphoreCreateMutex(); 

    tft_init(); 
    xTaskCreatePinnedToCore(ui_task, "ui", 4096, NULL, 4, NULL, tskNO_AFFINITY); 
} 

void status_list_set_pixel(int idx0, int on, uint32_t duration_ms){ 
    if (idx0<0 || idx0>=s_total) return; 
    xSemaphoreTake(s_lock, portMAX_DELAY); 
    s_on[idx0] = on ? 1 : 0; 
    s_remain_ms[idx0] = (on && duration_ms>0) ? duration_ms : 0; 
    xSemaphoreGive(s_lock); 
} 

void status_list_set_all(int on, uint32_t duration_ms){ 
    xSemaphoreTake(s_lock, portMAX_DELAY); 
    for (int i=0;i<s_total;i++){ 
        s_on[i] = on ? 1 : 0; 
        s_remain_ms[i] = (on && duration_ms>0) ? duration_ms : 0; 
    } 
    xSemaphoreGive(s_lock); 
} 

int status_list_get(int idx0, int *on, uint32_t *remain_ms){ 
    if (idx0<0 || idx0>=s_total) return -1; 
    xSemaphoreTake(s_lock, portMAX_DELAY); 
    if (on) *on = s_on[idx0]; 
    if (remain_ms) *remain_ms = s_remain_ms[idx0]; 
    xSemaphoreGive(s_lock); 
    return 0; 
} 

int status_list_total(void){ return s_total; }

void status_list_set_led_sync_cb(status_led_sync_cb_t cb){
    s_led_sync_cb = cb;
}