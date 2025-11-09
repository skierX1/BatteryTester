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

const char *TAG = "BATTERY_TESTER";

extern "C" void app_main(void)
{
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

    // Main loop
    //while (1) {
    //    ESP_LOGI(TAG, "System running - Free memory: %d bytes, Uptime: %s",
    //             esp_get_free_heap_size(), Webserver::get_uptime_string());
    //    vTaskDelay(pdMS_TO_TICKS(10000));
    //}
    
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
    VoltageReader vr;
    ESP_LOGI(TAG, "Starting multi-GPIO voltage reading");
    
    // Main reading loop
    while(true) {
        printf("\n==== CITAM NAPATIA CITAM AKO BLAZON ======================\n");
        vr.ReadVoltage();
        
        for (int i = 0; i < NUM_GPIOs; i++) {
            ESP_LOGI(TAG, "%s - Raw: %lu, Voltage: %.3fV, Calibrated: %s", 
                    vr.adc_channels[i].label,
                    vr.voltage[i].raw, 
                    vr.voltage[i].voltage,
                    vr.voltage[i].calibrated ? "Yes" : "No");
        }
        
        printf("================================================================\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    //------------------------------------------

    for (int i = 5; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("End of program ... now.\n");
    fflush(stdout);
    //esp_restart();

    Spiffs::Deactivate();
}
