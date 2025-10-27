#pragma once 
#include <stdint.h> 

// 初始化：total_leds = 74 
void status_list_init(int total_leds); 

// 设置单灯 开/关 + 持续时长（毫秒）；duration_ms=0 表示不计时（界面不显示剩余） 
void status_list_set_pixel(int idx0, int on, uint32_t duration_ms); 

// 设置全部 开/关 + 持续时长（毫秒） 
void status_list_set_all(int on, uint32_t duration_ms); 

// 读取某盏灯的状态（on=0/1，remain_ms 剩余毫秒） 
int  status_list_get(int idx0, int *on, uint32_t *remain_ms); 

// 总数
int  status_list_total(void);

// ---- 当显示模块内部触发(例如倒计时到0自动灭)时，通知外部去同步实际LED硬件 ----
typedef void (*status_led_sync_cb_t)(int idx0 /*0基像素索引，-1表示全部*/, int on /*0=灭,1=亮*/);

// 设置回调。传 NULL 可取消。
void status_list_set_led_sync_cb(status_led_sync_cb_t cb);