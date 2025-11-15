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

#include "driver/rmt.h"

#include "test.h"
#include "discharger.h"

const char *TAG = "BATTERY_TESTER";

extern VoltageReader* voltage_reader_;
extern Test* test_;
extern Discharger* discharger_;

#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define LED_PIN 48
#define RMT_CHANNEL RMT_CHANNEL_0

// Helper to convert 0/1 into WS2812 RMT pulse
static void ws2812_write_bit(rmt_item32_t *item, bool bit)
{
    if(bit) {
        // '1' bit: high 0.8us, low 0.45us
        item->level0 = 1;
        item->duration0 = 8;   // 0.1us units (800ns)
        item->level1 = 0;
        item->duration1 = 4;   // 0.45us
    } else {
        // '0' bit: high 0.4us, low 0.85us
        item->level0 = 1;
        item->duration0 = 4;
        item->level1 = 0;
        item->duration1 = 8;
    }
}

// Send single RGB value
void ws2812_send(rmt_channel_t channel, uint8_t r, uint8_t g, uint8_t b)
{
    rmt_item32_t items[24];
    int idx = 0;

    // WS2812 expects GRB order
    uint8_t colors[3] = {g, r, b};
    for(int c=0; c<3; c++) {
        for(int i=7; i>=0; i--) {
            ws2812_write_bit(&items[idx++], (colors[c] >> i) & 1);
        }
    }

    rmt_write_items(channel, items, 24, true);
    rmt_wait_tx_done(channel, portMAX_DELAY);
}

// Function to force LED off with proper reset
    void force_led_off() {
        ws2812_send(RMT_CHANNEL, 0, 0, 0);
        // WS2812 requires >50µs low signal to reset/latch
        vTaskDelay(pdMS_TO_TICKS(1)); // 1ms is more than enough
    }

    void proper_led_off( rmt_config_t &config) {
        // Send black color (all channels 0)
        ws2812_send(RMT_CHANNEL, 0, 0, 0);
        
        // CRITICAL: WS2812 requires >50µs LOW signal to reset/latch
        // Use gpio_set_level to force a long LOW period
        gpio_set_direction((gpio_num_t)LED_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1)); // 1ms = 1000µs (well over 50µs requirement)
        
        // Re-enable RMT
        rmt_config(&config);
        rmt_driver_install(config.channel, 0, 0);
    }

extern "C" void app_main(void)
{
    VoltageReader voltage_reader;
    Test test;
    Discharger discharger;

    voltage_reader_ = &voltage_reader;
    test_ = &test;
    discharger_ = &discharger;

    // Initialize spiffs partition
    if (!Spiffs::Activate()) {
        ESP_LOGI (TAG, "Flash memory activation failed !");
        return;
    }
      
    Spiffs::ListCSVFiles();
    
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
        // Toggle LEDs — invert if active-low
        //printf("\n==== CITAM NAPATIA CITAM AKO BLAZON ======================\n");
        voltage_reader.ReadVoltage();
        test_->run();
        
        //for (int i = 0; i < NUM_GPIOs; i++) {
        //    ESP_LOGI(TAG, "%s - Raw: %lu, Voltage: %.3fV, Calibrated: %s", 
        //            voltage_reader.adc_channels[i].label,
        //            voltage_reader.voltage[i].raw, 
        //            voltage_reader.voltage[i].voltage,
        //            voltage_reader.voltage[i].calibrated ? "Yes" : "No");
        //}
        
        //printf("================================================================\n");
        vTaskDelay(pdMS_TO_TICKS(50));
    }
 
    fflush(stdout);
    //esp_restart();

    Spiffs::Deactivate();

/* partitions.csv -> full flash size
    # Name,   Type, SubType, Offset,   Size,   Flags
nvs,      data, nvs,     0x9000,   0x4000,
phy_init, data, phy,     0xd000,   0x1000,
factory,  app,  factory, 0x10000,  0x138000,
spiffs,   data, spiffs,  0x148000, 0xEB8000,
*/
}
