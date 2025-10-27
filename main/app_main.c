#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "led/led_ws2812.h"
#include "led/led_effects.h"
#include "net_mqtt.h"
#include "display/status_list.h"  // 新的状态列表显示接口
#include "util/color.h"

static const char *TAG = "app_main";
static TimerHandle_t static_ip_timer = NULL;
static esp_netif_t *sta_netif = NULL;

// 声明 
void net_mqtt_start(void); 

static bool s_mqtt_started = false;

// WiFi事件处理函数
static void wifi_event_handler(void* arg, esp_event_base_t event_base, 
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA启动");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "WiFi连接成功");
        
        // 启动5秒定时器，用于DHCP超时检测
        static_ip_timer = xTimerCreate("static_ip_timer", pdMS_TO_TICKS(5000), pdFALSE, 
                                      (void*)0, NULL);
        xTimerStart(static_ip_timer, 0);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "WiFi断开连接，原因: %d", event->reason);
        
        // 停止静态IP定时器
        if (static_ip_timer) {
            xTimerStop(static_ip_timer, 0);
            xTimerDelete(static_ip_timer, 0);
            static_ip_timer = NULL;
        }
        
        // 重置MQTT启动标志
        s_mqtt_started = false;
        
        // 5秒后重连
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "获取到IP地址: " IPSTR, IP2STR(&event->ip_info.ip));
        
        // 停止静态IP定时器
        if (static_ip_timer) {
            xTimerStop(static_ip_timer, 0);
            xTimerDelete(static_ip_timer, 0);
            static_ip_timer = NULL;
        }
        
        // 只在拿到IP后启动MQTT，并且只启动一次
        if (!s_mqtt_started){
            s_mqtt_started = true;
            net_mqtt_start();
        }
    }
}

// LED同步回调函数 - 将UI状态同步到实际LED灯带
static void on_led_sync_from_ui(int idx0, int on){
    if (on){
        // 若未来需要"到期转亮"也能复用
        led_set_effect_solid((rgb_color_t){255,0,0}, idx0);
    }else{
        // 到期自动灭：把该像素关掉（黑色）
        led_set_effect_solid((rgb_color_t){0,0,0}, idx0);
    }
}

void app_main(void)
{
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 创建STA网络接口
    sta_netif = esp_netif_create_default_wifi_sta();

    // 注册WiFi事件处理器
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                                        &wifi_event_handler, NULL, NULL));

    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // 配置WiFi连接 - 使用配置文件中的WiFi信息
    wifi_config_t wifi_config = {0};
    strcpy((char*)wifi_config.sta.ssid, CONFIG_RGBMIN_WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, CONFIG_RGBMIN_WIFI_PASS);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 初始化灯带（74颗）- 使用GPIO38
    led_init_ws2812(CONFIG_RGBMIN_WS2812_GPIO, CONFIG_RGBMIN_WS2812_COUNT);

    // 初始化显示列表（74）
    status_list_init(CONFIG_RGBMIN_WS2812_COUNT);

    // 注册LED同步回调
    status_list_set_led_sync_cb(on_led_sync_from_ui);

    // 默认全灭
    status_list_set_all(0, 0);
    led_set_effect_off();

    ESP_LOGI(TAG, "Hello world!");
}