idf_component_register(
   SRCS 
     "app_main.c" 
     "net_mqtt.c" 
     "util/color.c" 
     "led/led_effects.c" 
     "led/led_ws2812.c" 
     "display/tft_ili9341.c" 
     "display/status_list.c" 
   INCLUDE_DIRS 
     "." 
     "util" 
     "led" 
     "display" 
   REQUIRES 
     driver esp_timer esp_wifi esp_event nvs_flash mqtt json led_strip 
 )
