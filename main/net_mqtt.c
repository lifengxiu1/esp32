#include "esp_log.h" 
#include "mqtt_client.h" 
#include "cJSON.h" 
#include <string.h> 
#include <strings.h> 
#include <stdlib.h> 
#include <time.h> 
#include <inttypes.h> 

#include "led/led_effects.h" 
#include "util/color.h" 
#include "display/status_list.h" 

static const char *TAG = "NET_MQTT"; 
static esp_mqtt_client_handle_t client = NULL;

// ★ 按你的电脑热点 IP/端口改这里 
#define BROKER_HOST "192.168.158.148" 
#define BROKER_PORT 1883 

// --- 将当前全量状态打包为 JSON 并上报 --- 
static void publish_full_state(const char* reason){ 
    cJSON *root = cJSON_CreateObject(); 
    cJSON_AddBoolToObject(root, "ok", true); 
    cJSON_AddStringToObject(root, "reason", reason ? reason : ""); 
    cJSON_AddNumberToObject(root, "ts", (double)time(NULL)); 

    cJSON *arr = cJSON_CreateArray(); 
    int total = status_list_total(); 
    for (int i=0;i<total;i++){ 
        int on=0; uint32_t ms=0; 
        status_list_get(i,&on,&ms); 
        cJSON *o = cJSON_CreateObject(); 
        cJSON_AddNumberToObject(o, "id", i+1);           // 1..74 
        cJSON_AddNumberToObject(o, "on", on ? 1 : 0);    // 1=亮,0=灭 
        if (on && ms>0) cJSON_AddNumberToObject(o, "remain_ms", (double)ms); 
        else            cJSON_AddNullToObject(o, "remain_ms");   // 关闭或未设置时长为空 
        cJSON_AddItemToArray(arr, o); 
    } 
    cJSON_AddItemToObject(root, "lights", arr); 

    char *payload = cJSON_PrintUnformatted(root); 
    esp_mqtt_client_publish(client, "rgb/ack", payload, 0, 1, 0); 
    cJSON_free(payload); 
    cJSON_Delete(root); 
} 

// --- 每秒推送一次状态的任务 --- 
static void telemetry_task(void*){ 
    while(1){ 
        vTaskDelay(pdMS_TO_TICKS(1000)); 
        if (client) publish_full_state("tick"); 
    } 
} 

// --- 处理控制命令 --- 
static void handle_message(const char *data, int len) { 
    char *buf = (char*)malloc(len + 1); 
    if (!buf) { return; } 
    memcpy(buf, data, len); buf[len]=0; 

    cJSON *root = cJSON_Parse(buf); 
    free(buf); 
    if (!root) { publish_full_state("bad_json"); return; } 

    const cJSON *cmd = cJSON_GetObjectItem(root, "cmd"); 
    const cJSON *effect = cJSON_GetObjectItem(root, "effect"); 
    const cJSON *pixel  = cJSON_GetObjectItem(root, "pixel");       // 0基 
    const cJSON *dur    = cJSON_GetObjectItem(root, "duration_ms"); // 点亮时长（毫秒） 

    if (!cJSON_IsString(cmd) || strcmp(cmd->valuestring,"set")!=0 
     || !cJSON_IsString(effect)) { 
        cJSON_Delete(root); publish_full_state("bad_cmd_or_effect"); return; 
    } 

    int has_pixel = cJSON_IsNumber(pixel); 
    int idx0 = has_pixel ? (int)pixel->valuedouble : -1; 
    if (has_pixel && (idx0<0 || idx0>=status_list_total())){ 
        cJSON_Delete(root); publish_full_state("bad_pixel"); return; 
    } 

    uint32_t duration_ms = 0; 
    if (cJSON_IsNumber(dur) && dur->valuedouble>0) { 
        duration_ms = (uint32_t)dur->valuedouble; 
    } 

    if (!strcasecmp(effect->valuestring,"off")){ 
        if (has_pixel){ 
            status_list_set_pixel(idx0, 0, 0); 
            led_set_effect_solid((rgb_color_t){0,0,0}, idx0); 
        } else { 
            status_list_set_all(0, 0); 
            led_set_effect_off(); 
        } 
        publish_full_state("ok"); 
        cJSON_Delete(root); 
        return; 
    } 

    if (!strcasecmp(effect->valuestring,"solid")){ 
        if (has_pixel){ 
            status_list_set_pixel(idx0, 1, duration_ms); 
            led_set_effect_solid((rgb_color_t){255,0,0}, idx0); // 红 
        } else { 
            status_list_set_all(1, duration_ms); 
            led_set_effect_solid((rgb_color_t){255,0,0}, -1);  // 全红 
        } 
        publish_full_state("ok"); 
        cJSON_Delete(root); 
        return; 
    } 

    cJSON_Delete(root); 
    publish_full_state("unsupported_effect"); 
} 

// 事件回调（补充错误细节） 
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){
    esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)event_data;
    switch (id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected, subscribe rgb/ctrl");
        esp_mqtt_client_subscribe(e->client, "rgb/ctrl", 1);
        publish_full_state("online");
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "RX topic=%.*s len=%d", e->topic_len, e->topic, e->data_len);
        handle_message(e->data, e->data_len);
        break;
    case MQTT_EVENT_ERROR: {
        esp_mqtt_error_codes_t *er = e->error_handle;
        ESP_LOGE(TAG,
          "ERROR type=%d conn_rc=%d esp_tls_err=0x%x tls_stack=0x%x sock_errno=%d",
          er?er->error_type:-1, er?er->connect_return_code:-1,
          er?er->esp_tls_last_esp_err:0, er?er->esp_tls_stack_err:0,
          er?er->esp_transport_sock_errno:0);
        break; }
    default:
        break;
    }
} 

void net_mqtt_start(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker = {
            .address = {
                .hostname  = BROKER_HOST,
                .port      = BROKER_PORT,
                .transport = MQTT_TRANSPORT_OVER_TCP,   // ★ 强制明文 TCP
            },
            // ★ 不要设置任何 verification/certificate 字段
        },
        .session = { .keepalive = 30, .disable_clean_session = false },
        .network = { .disable_auto_reconnect = false, .reconnect_timeout_ms = 10000, .timeout_ms = 5000 },
        .credentials = { .client_id = "esp32-rgb" },
    };

    ESP_LOGI(TAG, "CONNECT %s:%" PRIu32 " over TCP", cfg.broker.address.hostname, cfg.broker.address.port);

    client = esp_mqtt_client_init(&cfg);
    if (!client) { ESP_LOGE(TAG, "client init failed"); return; }
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK) { ESP_LOGE(TAG, "start failed %s", esp_err_to_name(err)); }
    xTaskCreatePinnedToCore(telemetry_task, "telemetry", 4096, NULL, 4, NULL, tskNO_AFFINITY);
}