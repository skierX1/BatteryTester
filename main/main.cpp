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



// WiFi event handler for ESP-IDF v5.5.1
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "WiFi AP started");
                break;
                
            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "WiFi AP stopped");
                break;
                
            case WIFI_EVENT_AP_STACONNECTED:
                {
                    wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                    //ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d",
                             //MAC2STR(event->mac), event->aid);
                }
                break;
                
            case WIFI_EVENT_AP_STADISCONNECTED:
                {
                    wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                    //ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d",
                             //MAC2STR(event->mac), event->aid);
                }
                break;
                
            default:
                break;
        }
    }
}

// Initialize WiFi as Access Point
void wifi_init_softap(void) {
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create default WiFi AP
    esp_netif_create_default_wifi_ap();

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Configure AP settings
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .ssid_len = static_cast<uint8_t>(strlen(WIFI_SSID)),
            .channel = 1,
            .authmode = wifi_auth_mode_t::WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = MAX_STA_CONN
        },
    };
    
    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    // Set WiFi mode and start
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP started. SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
}



//------------

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
    wifi_init_softap();

    // Start web server
    Webserver::start_webserver();

    // Print connection info
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, "Access Point Created:");
    ESP_LOGI(TAG, "  SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "  Password: %s", WIFI_PASS);
    ESP_LOGI(TAG, "  IP Address: %s", WIFI_IP);
    ESP_LOGI(TAG, "====================================");

    // Main loop
    while (1) {
        ESP_LOGI(TAG, "System running - Free memory: %d bytes, Uptime: %s",
                 esp_get_free_heap_size(), Webserver::get_uptime_string());
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    ///////////////////////////////////
  
    // Try the new ADC API for ESP32-C6
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    
    ret = adc_oneshot_new_unit(&init_config1, &adc1_handle);
    if (ret == ESP_OK) {
        printf("ADC unit initialized successfully!\n");
    } else {
        printf("ADC init failed: %d\n", ret);
    }
    
    while (1) {
        printf("System running...\n");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    //------------------------------------------
    VoltageReader vr;
    ESP_LOGI(TAG, "Starting multi-GPIO voltage reading");
    
    // Main reading loop
    for(int i=0; i<50000; i++) {
        ESP_LOGI(TAG, "\n==== CITAM NAPATIA CITAM AKO BLAZON (%d)======================", i);
        vr.ReadVoltage();
        
        for (int i = 0; i < NUM_GPIOs; i++) {
            ESP_LOGI(TAG, "%s - Raw: %lu, Voltage: %.3fV, Calibrated: %s", 
                    vr.adc_channels[i].label,
                    vr.voltage[i].raw, 
                    vr.voltage[i].voltage,
                    vr.voltage[i].calibrated ? "Yes" : "No");
        }
        
        ESP_LOGI(TAG, "=============================\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    

    
    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
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

    for (int i = 5; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("End of program ... now.\n");
    fflush(stdout);
    //esp_restart();

    Spiffs::Deactivate();
}
