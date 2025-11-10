#ifndef __WEBSERVER_H
#define __WEBSERVER_H

#include <string.h>
#include "esp_event.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "files_rw.h"
#include "esp_log.h"
#include <esp_log.h>
#include "voltage.h"
#include "test.h"

extern const char *TAG;
extern VoltageReader* voltage_reader_;
extern Test* test_;

class Test;

class Webserver {
public:
    Webserver()  {};
    ~Webserver() {};

    static void start_webserver(void) {
        httpd_handle_t server = NULL;
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.lru_purge_enable = true;

        ESP_LOGI(TAG, "Starting HTTP server on port: '%d'", config.server_port);
        if (httpd_start(&server, &config) == ESP_OK) {
            // Register URI handlers
            httpd_register_uri_handler(server, &Webserver::root);
            httpd_register_uri_handler(server, &Webserver::test);
            httpd_register_uri_handler(server, &Webserver::data);
            httpd_register_uri_handler(server, &Webserver::startbtn);
            httpd_register_uri_handler(server, &Webserver::stopbtn);
            httpd_register_uri_handler(server, &Webserver::download);
            ESP_LOGI(TAG, "HTTP server started successfully");
        } else {
            ESP_LOGE(TAG, "Error starting HTTP server!");
        }
        
    };

    static char* get_uptime_string() {
        static char uptime_str[20];
        unsigned long seconds = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
        unsigned long minutes = seconds / 60;
        unsigned long hours = minutes / 60;
        snprintf(uptime_str, sizeof(uptime_str), "%02lu:%02lu:%02lu", hours, minutes % 60, seconds % 60);
        return uptime_str;
    };
 
    // HTTP request handler for root page
    static esp_err_t root_get_handler(httpd_req_t *req) {
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
    };
    static constexpr httpd_uri_t root = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = root_get_handler,
        .user_ctx  = NULL
    };

    // HTTP request handler for test page
    static esp_err_t test_get_handler(httpd_req_t *req) {
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
            char* uptime = Webserver::get_uptime_string();
            memcpy(ptr, uptime, strlen(uptime));
            ptr += strlen(uptime);
        }

        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, html, strlen(html));
        free(html);
        
        return ESP_OK;
    };
    static constexpr httpd_uri_t test = {
        .uri       = "/test",
        .method    = HTTP_GET,
        .handler   = Webserver::test_get_handler,
        .user_ctx  = NULL
    };

    static esp_err_t download_get_handler(httpd_req_t *req)
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
    };
    static constexpr httpd_uri_t download = {
        .uri       = "/download",
        .method    = HTTP_GET,
        .handler   = Webserver::download_get_handler,
        .user_ctx  = NULL
    };

    
    static esp_err_t data_get_handler(httpd_req_t *req) {
        char json_response[256];

        ESP_LOGI(TAG, "... DATA REQUEST RECEIVED ...");
        ESP_LOGI(TAG, "%s", req->uri);

        snprintf(json_response, sizeof(json_response),
                "{\"voltage\":%.2f,\"cell1\":%.2f,\"cell2\":%.2f,\"current\":%.2f,\"countdown\":%d,\"test_running\":%d}",
                voltage_reader_->voltage[1].voltage,
                voltage_reader_->voltage[0].voltage,
                voltage_reader_->voltage[1].voltage - voltage_reader_->voltage[0].voltage,
                test_->test_running ? voltage_reader_->voltage[1].voltage / 0.2 : 0,
                test_->countdown,
                test_->test_running ? 1 : 0
                );
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_response, strlen(json_response));
        
        return ESP_OK;
    };
    static constexpr httpd_uri_t data = {
        .uri       = "/data",
        .method    = HTTP_GET,
        .handler   = Webserver::data_get_handler,
        .user_ctx  = NULL
    };

    static esp_err_t startbtn_handler(httpd_req_t *req) {
        char json_response[256];

        ESP_LOGI(TAG, "... STARTBTN RECEIVED ...");
        ESP_LOGI(TAG, "%s", req->uri);

        
        const char* key = "name=";
        char* name = strstr(req->uri, key);  // find "name=" in the string
        if (name == nullptr) {
            printf("Cannot extract name !!!!!");                
            test_->test_name = "";
        }
        else {
            name += strlen(key);          // move pointer past "name="
            printf("Extracted name: %s\n", name);
            test_->test_name = name;
            test_->test_start = true;
        }

        snprintf(json_response, sizeof(json_response),
                "{\"status\":\"%s\"}",
                "ok");
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_response, strlen(json_response));
        
        return ESP_OK;
    };
    static constexpr httpd_uri_t startbtn = {
        .uri       = "/startbtn",
        .method    = HTTP_GET,
        .handler   = Webserver::startbtn_handler,
        .user_ctx  = NULL
    };

    static esp_err_t stopbtn_handler(httpd_req_t *req) {
        char json_response[256];

        ESP_LOGI(TAG, "... STOPBTN RECEIVED ...");
        ESP_LOGI(TAG, "%s", req->uri);

        snprintf(json_response, sizeof(json_response),
                "{\"status\":\"%s\"}",
                "ok");
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_response, strlen(json_response));
        
        test_->test_stop = true;

        return ESP_OK;
    };
    static constexpr httpd_uri_t stopbtn = {
        .uri       = "/stopbtn",
        .method    = HTTP_GET,
        .handler   = Webserver::stopbtn_handler,
        .user_ctx  = NULL
    };
};


#endif //__WEBSERVER_H