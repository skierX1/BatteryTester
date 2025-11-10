#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

#include "driver/gpio.h"
#include "driver/adc.h"
#include <esp_adc_cal.h>
#include "esp_log.h"

#include <driver/adc.h>
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc_cal.h"
#include <esp_log.h>

#include "voltage.h"

#include "esp_spiffs.h"
#include "esp_adc/adc_oneshot.h"

#include "defines.h"
/////////////////////////////////////////////////////

#include <string.h>
#include "esp_event.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "files_rw.h"
#include "webserver.h"
#include "wifi.h"

#include "test.h"

const char *TAG = "BATTERY_TESTER";
extern VoltageReader* voltage_reader_;
extern Test* test_;

#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_GPIO    8
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_TIMER   LEDC_TIMER_0

extern "C" void app_main(void)
{

    
    // just turning on, not blinking .. not possible to set it off !!!
    /*
    // 1. Configure LEDC timer
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,        
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    // 2. Configure LEDC channel
    ledc_channel_config_t channel = {
        .gpio_num = LED_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 255,   // start OFF (active-low)
        .hpoint = 0
    };
    ledc_channel_config(&channel);

    // 3. Blink loop
    bool led_on = false;
    while (true) {
        printf("Tick  ......");
        if (led_on) {
            // Turn LED ON (active-low)            
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 0);
        } else {
            // Turn LED OFF             
            //ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 1);
        }
        //vTaskDelay(pdMS_TO_TICKS(200)); // 500 ms delay
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);

        led_on = !led_on;  // toggle state
        vTaskDelay(pdMS_TO_TICKS(500)); // 500 ms delay
    }
*/
    VoltageReader voltage_reader;
    Test test;
    voltage_reader_ = &voltage_reader;
    test_ = &test;

    // Initialize spiffs partition
    if (!Spiffs::Activate()) {
        ESP_LOGI (TAG, "Flash memory activation failed !");
        return;
    }
      
    // Initialize NVS partition
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    ESP_LOGI(TAG, "ESP32-C6 Web Server Starting...");
    Wifi::wifi_init_softap();

    // Start web server
    Webserver::start_webserver();

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("************************************************************************************************");
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    printf("************************************************************************************************");

    //------------------------------------------
    
    ESP_LOGI(TAG, "Starting multi-GPIO voltage reading");

    // Main reading loop

    while(true) {
        test_->run();
        // Toggle LEDs — invert if active-low
        //printf("\n==== CITAM NAPATIA CITAM AKO BLAZON ======================\n");
        voltage_reader.ReadVoltage();
        
        //for (int i = 0; i < NUM_GPIOs; i++) {
        //    ESP_LOGI(TAG, "%s - Raw: %lu, Voltage: %.3fV, Calibrated: %s", 
        //            voltage_reader.adc_channels[i].label,
        //            voltage_reader.voltage[i].raw, 
        //            voltage_reader.voltage[i].voltage,
        //            voltage_reader.voltage[i].calibrated ? "Yes" : "No");
        //}
        
        //printf("================================================================\n");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
 
    fflush(stdout);
    //esp_restart();

    Spiffs::Deactivate();
}
