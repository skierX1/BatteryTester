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

// WiFi configuration
#define WIFI_SSID      "ESP32-C6-Batt"
#define WIFI_PASS      ""
#define WIFI_IP        "192.168.4.1"
#define MAX_STA_CONN   4

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
    ESP_LOGI(TAG, ".......... ROOT PAGE REQUEST RECEIVED ..............");
    ESP_LOGI(TAG, "%s", req->uri);
    
    ESP_LOGI(TAG, ".. Response by redirect ..............");

    char redirtxt[512];
    sprintf(redirtxt,"%s/test", WIFI_IP);
    char *html = (char*)malloc(512);

    const char* status = "302 Found";
    const char* msg    = "Redirecting to testing page";
  
    httpd_resp_set_status(req, status);    
    httpd_resp_set_type(req, HTTPD_TYPE_TEXT);    
    httpd_resp_set_hdr(req, "Location", redirtxt);
    
    strcpy(html,redirtxt);        
    httpd_resp_send(req, html, strlen(html));
    free(html);    

    return ESP_OK;
}

esp_err_t download_get_handler(httpd_req_t *req)
{
    const char *response_data =
        "time,temperature,humidity\n"
        "12:00,24.3,60\n"
        "13:00,25.1,58\n";

    // Set headers
    httpd_resp_set_type(req, "text/csv");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"data.csv\"");

    // Send data
    httpd_resp_send(req, response_data, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


// HTTP request handler for test page
esp_err_t test_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, ".......... TEST PAGE REQUEST RECEIVED ..............");
    ESP_LOGI(TAG, "%s", req->uri);

    FILE *f = Spiffs::Open("webpage.html");
    if (f == NULL) {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);  
    long size = ftell(f);
    rewind(f);

    char *html = (char*)malloc(size + 500);
    if (!html) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    html[0] = '\0';  // start with an empty string

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        strcat(html, line);  // append each line
    }
    
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
        memcpy(ptr, WIFI_IP, strlen(WIFI_IP));
        ptr +=  strlen(WIFI_IP);
    }
    
    // Free memory
    ptr = html;
    while ((ptr = strstr(ptr, "%FREE_MEMORY%")) != NULL) {
        snprintf(temp, sizeof(temp), "%ld", esp_get_free_heap_size());
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

    ESP_LOGI(TAG, "... DATA REQUEST RECEIVED ...");
    ESP_LOGI(TAG, "%s", req->uri);

    snprintf(json_response, sizeof(json_response),
             "{\"voltage\":%.2f,\"cell1\":%.2f,\"cell2\":%.2f,\"current\":%.2f,\"countdown\":%d}",
             3.84,
             4.11,
             8.34,
            39.2,
            4);
    
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

static const httpd_uri_t test = {
    .uri       = "/test",
    .method    = HTTP_GET,
    .handler   = test_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t data = {
    .uri       = "/data",
    .method    = HTTP_GET,
    .handler   = data_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t download = {
    .uri       = "/download",
    .method    = HTTP_GET,
    .handler   = download_get_handler,
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

// Start HTTP server
void start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting HTTP server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &test);
        httpd_register_uri_handler(server, &data);
        httpd_register_uri_handler(server, &download);
        ESP_LOGI(TAG, "HTTP server started successfully");
    } else {
        ESP_LOGE(TAG, "Error starting HTTP server!");
    }
}

//------------

extern "C" void app_main(void)
{

  if (!Spiffs::Activate()) {
    ESP_LOGI (TAG, "Flash memory activation failed !");
    return;
  }
  
    ///////////////////////////////////
// Initialize NVS
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
    start_webserver();

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
                 esp_get_free_heap_size(), get_uptime_string());
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
