
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

#include "./voltage.h"

//------------
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"

// For ADC in ESP32-C6, try including through driver
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

/////////////////////////////////////////////////////
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"

// WiFi configuration
#define WIFI_SSID      "ESP32-C6-Sensor"
#define WIFI_PASS      "123456789"
#define MAX_STA_CONN   4

// HTML web page
static const char* HTML_PAGE = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>ESP32-C6 Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { 
      font-family: Arial, sans-serif; 
      text-align: center; 
      margin: 0; 
      padding: 20px; 
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      color: white;
    }
    .container {
      max-width: 600px;
      margin: 0 auto;
      background: rgba(255,255,255,0.1);
      padding: 30px;
      border-radius: 15px;
      backdrop-filter: blur(10px);
      box-shadow: 0 8px 32px rgba(0,0,0,0.1);
    }
    .card {
      background: rgba(255,255,255,0.2);
      padding: 20px;
      margin: 15px 0;
      border-radius: 10px;
      border: 1px solid rgba(255,255,255,0.3);
    }
    .value {
      font-size: 2.5em;
      font-weight: bold;
      margin: 10px 0;
      color: #fff;
    }
    .label {
      font-size: 1.2em;
      opacity: 0.8;
    }
    button {
      background: #4CAF50;
      color: white;
      border: none;
      padding: 12px 24px;
      border-radius: 25px;
      font-size: 16px;
      cursor: pointer;
      margin: 10px;
      transition: all 0.3s;
    }
    button:hover {
      background: #45a049;
      transform: translateY(-2px);
    }
    .status {
      margin: 20px 0;
      padding: 10px;
      border-radius: 5px;
      background: rgba(255,255,255,0.2);
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🚀 ESP32-C6 Web Server</h1>
    <div class="status">
      <p>Connected to: <strong>%SSID%</strong></p>
      <p>IP Address: <strong>%IP%</strong></p>
    </div>
    
    <div class="card">
      <div class="label">System Information</div>
      <div class="value" id="freeMemory">%FREE_MEMORY%</div>
      <div class="label">bytes free</div>
    </div>

    <div class="card">
      <div class="label">Uptime</div>
      <div class="value" id="uptime">%UPTIME%</div>
      <div class="label">hours:minutes:seconds</div>
    </div>

    <button onclick="refreshData()">🔄 Refresh</button>
    <button onclick="toggleAutoRefresh()" id="autoRefreshBtn">⏰ Auto-Refresh (5s)</button>
    
    <div style="margin-top: 30px; font-size: 0.9em; opacity: 0.7;">
      <p>ESP32-C6 WROOM Kit | ESP-IDF v5.5.1</p>
    </div>
  </div>

  <script>
    let autoRefreshInterval = null;

    function refreshData() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('freeMemory').textContent = data.free_memory;
          document.getElementById('uptime').textContent = data.uptime;
        })
        .catch(error => console.error('Error:', error));
    }

    function toggleAutoRefresh() {
      const button = document.getElementById('autoRefreshBtn');
      
      if (autoRefreshInterval) {
        clearInterval(autoRefreshInterval);
        autoRefreshInterval = null;
        button.textContent = '⏰ Auto-Refresh (5s)';
        button.style.background = '#4CAF50';
      } else {
        refreshData();
        autoRefreshInterval = setInterval(refreshData, 5000);
        button.textContent = '⏹️ Stop Auto-Refresh';
        button.style.background = '#f44336';
      }
    }

    setInterval(refreshData, 10000);
    refreshData();
  </script>
</body>
</html>
)rawliteral";

// Get uptime string
char* get_uptime_string() {
    static char uptime_str[20];
    unsigned long seconds = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    snprintf(uptime_str, sizeof(uptime_str), "%02lu:%02lu:%02lu", hours, minutes % 60, seconds % 60);
    return uptime_str;
}

// HTTP request handler for root page
esp_err_t root_get_handler(httpd_req_t *req) {
    char *html = (char*)malloc(strlen(HTML_PAGE) + 500);
    if (!html) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    strcpy(html, HTML_PAGE);
    
    // Replace placeholders
    char temp[50];
    char *ptr = html;
    
    // SSID
    while ((ptr = strstr(ptr, "%SSID%")) != NULL) {
        memcpy(ptr, WIFI_SSID, strlen(WIFI_SSID));
        ptr += strlen(WIFI_SSID);
    }
    
    // IP address
    ptr = html;
    while ((ptr = strstr(ptr, "%IP%")) != NULL) {
        memcpy(ptr, "192.168.4.1", 11);
        ptr += 11;
    }
    
    // Free memory
    ptr = html;
    while ((ptr = strstr(ptr, "%FREE_MEMORY%")) != NULL) {
        snprintf(temp, sizeof(temp), "%d", esp_get_free_heap_size());
        memcpy(ptr, temp, strlen(temp));
        ptr += strlen(temp);
    }
    
    // Uptime
    ptr = html;
    while ((ptr = strstr(ptr, "%UPTIME%")) != NULL) {
        char* uptime = get_uptime_string();
        memcpy(ptr, uptime, strlen(uptime));
        ptr += strlen(uptime);
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, strlen(html));
    free(html);
    
    return ESP_OK;
}

// HTTP request handler for JSON data
esp_err_t data_get_handler(httpd_req_t *req) {
    char json_response[256];
    snprintf(json_response, sizeof(json_response),
             "{\"free_memory\":%d,\"uptime\":\"%s\",\"uptime_ms\":%lu}",
             esp_get_free_heap_size(),
             get_uptime_string(),
             xTaskGetTickCount() * portTICK_PERIOD_MS);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, strlen(json_response));
    
    return ESP_OK;
}

// HTTP URI handlers
static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t data = {
    .uri       = "/data",
    .method    = HTTP_GET,
    .handler   = data_get_handler,
    .user_ctx  = NULL
};

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
                    ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d",
                             MAC2STR(event->mac), event->aid);
                }
                break;
                
            case WIFI_EVENT_AP_STADISCONNECTED:
                {
                    wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                    ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d",
                             MAC2STR(event->mac), event->aid);
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
    wifi_config_t wc;

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = 1,
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
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

// Start HTTP server
void start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting HTTP server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &data);
        ESP_LOGI(TAG, "HTTP server started successfully");
    } else {
        ESP_LOGE(TAG, "Error starting HTTP server!");
    }
}

//------------

extern "C" void app_main(void)
{
    printf("ESP32-C6 ADC Test with new API\n");
    
    // Try the new ADC API for ESP32-C6
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    
    esp_err_t ret = adc_oneshot_new_unit(&init_config1, &adc1_handle);
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
}
