#ifndef __WEBSERVER_H
#define __WEBSERVER_H


#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <cmath>

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
            httpd_register_uri_handler(server, &Webserver::filedir);
            
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

    /*static esp_err_t download_get_handler(httpd_req_t *req)
    {

        ESP_LOGI(TAG, "... DOWNLOAD_GETHANDLER RECEIVED ...");
        ESP_LOGI(TAG, "%s", req->uri);

        std::string filename;
        
        const char* key = "file=";
        char* name = strstr(req->uri, key);  // find "file=" in the string
        if (name == nullptr) {
            ESP_LOGE(TAG, "Cannot extract filename !!!!!");                
            httpd_resp_send_500(req);
            return ESP_FAIL;                        
        }
        else {
           name += strlen(key);  // move pointer past "file="
            filename = name;

            // ✅ Trim off any other query parameters (&...) or spaces
            size_t amp = filename.find('&');
            if (amp != std::string::npos) filename = filename.substr(0, amp);
            size_t space = filename.find(' ');
            if (space != std::string::npos) filename = filename.substr(0, space);

             // ✅ Trim any trailing \r or \n
            while (!filename.empty() && (filename.back() == '\n' || filename.back() == '\r'))
                filename.pop_back();

            ESP_LOGI(TAG, "Extracted filename: %s", filename.c_str());
        }

        std::ostringstream csv_response;
        if (!GetCsvResponse(filename, csv_response)) {
            ESP_LOGE(TAG, "Cannot extract filename !!!!!");
            httpd_resp_send_500(req);
            return ESP_FAIL;                        
        }
            
        // Set headers
        httpd_resp_set_type(req, "text/csv");
        std::ostringstream attachment_hdr;
        attachment_hdr << "attachment; filename=\"" << filename << "\"";
        httpd_resp_set_hdr(req, "Content-Disposition", attachment_hdr.str().c_str());

        // Send data
        httpd_resp_send(req, csv_response.str().c_str(), HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    };*/

    static esp_err_t download_get_handler(httpd_req_t *req)
    {
        ESP_LOGI(TAG, "... DOWNLOAD_GETHANDLER RECEIVED ...");
        ESP_LOGI(TAG, "%s", req->uri);

        std::string filename;

        const char* key = "file=";
        char* name = strstr(req->uri, key);
        if (name == nullptr) {
            ESP_LOGE(TAG, "Cannot extract filename!");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        } 
        name += strlen(key);
        filename = name;

        // Trim trailing parameters/spaces/newlines
        size_t amp = filename.find('&');
        if (amp != std::string::npos) filename = filename.substr(0, amp);
        size_t space = filename.find(' ');
        if (space != std::string::npos) filename = filename.substr(0, space);
        while (!filename.empty() && (filename.back() == '\n' || filename.back() == '\r'))
            filename.pop_back();

        ESP_LOGI(TAG, "Extracted filename: %s", filename.c_str());

        std::ostringstream csv_response;
        if (!GetCsvResponse(filename, csv_response)) {
            ESP_LOGE(TAG, "Cannot generate CSV for file: %s", filename.c_str());
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        // ✅ Set headers before sending any data
        httpd_resp_set_type(req, "text/csv");

        char content_disp[128];
        snprintf(content_disp, sizeof(content_disp), "attachment; filename=\"%s\"", filename.c_str());
        httpd_resp_set_hdr(req, "Content-Disposition", content_disp);

        httpd_resp_send(req, csv_response.str().c_str(), HTTPD_RESP_USE_STRLEN);

        return ESP_OK;
    }
    static constexpr httpd_uri_t download = {
        .uri       = "/download",
        .method    = HTTP_GET,
        .handler   = Webserver::download_get_handler,
        .user_ctx  = NULL
    };

    
    static bool GetCsvResponse (const std::string &filename, std::ostringstream &csv_response) {
        std::string file_name = "/spiffs/data/" + filename;
        FILE *f = fopen(file_name.c_str(), "r");
        if(f==NULL) {          
            return false;
        }
        
        char line[64];
        csv_response << "Time[s],Battery[V],Cell1[V],Cell2[V],Current[A],Resistance[Ohm]\n";
        bool first_line{true};
        while (fgets(line, sizeof(line), f)) {
            std::stringstream ss(line);
            std::string item;
            std::vector<std::string> tokens;

            while (std::getline(ss, item, ',')) {
                // Remove potential newline at end
                if (!item.empty() && item.back() == '\n') item.pop_back();
                    tokens.push_back(item);
            }

            if (first_line) {
                first_line = false;
                continue;
            }
            
            if (tokens.size() >= 4) {
                float time   = std::stof(tokens[0]);
                float Vbatt  = std::stof(tokens[1]);
                float Vcell1 = std::stof(tokens[2]);
                float R      = std::stof(tokens[3]);
//                ESP_LOGI(TAG, "Read: id=%.1f, v1=%.2f, v2=%.2f, v3=%.2f", time, Vbatt, Vcell1, R);

                //calculated fields
                float Vcell2  = Vbatt - Vcell1;
                float Current = Vbatt / R;

                csv_response << std::fixed << std::setprecision(3) << time << ",";
                csv_response << std::fixed << std::setprecision(3) << Vbatt << ",";
                csv_response << std::fixed << std::setprecision(3) << Vcell1 << ",";
                csv_response << std::fixed << std::setprecision(3) << Vcell2 << ",";
                csv_response << std::fixed << std::setprecision(3) << Current << ",";
                csv_response << std::fixed << std::setprecision(3) << R << "\n";
            }
        }

        fclose(f);

        return true;
    }
    
    static esp_err_t data_get_handler(httpd_req_t *req) {
        char json_response[256];

        ESP_LOGI(TAG, "... DATA REQUEST RECEIVED ...");
        ESP_LOGI(TAG, "%s", req->uri);

        double time_sec = std::round(static_cast<double>(test_->test_max_time_sec - test_->test_time_sec_) / 1000000.0);

        std::ostringstream time_str_stream;
        time_str_stream << std::fixed << std::setprecision(0) << time_sec;

        snprintf(json_response, sizeof(json_response),
                "{\"voltage\":%.3f,\"cell1\":%.3f,\"cell2\":%.3f,\"current\":%.3f,\"countdown\":%s,\"test_running\":%d,\"refreshlist\":%d}",
                voltage_reader_->voltage[1].voltage,
                voltage_reader_->voltage[0].voltage,
                voltage_reader_->voltage[1].voltage - voltage_reader_->voltage[0].voltage,
                test_->test_running ? voltage_reader_->voltage[1].voltage / 0.2 : 0,
                time_str_stream.str().c_str(),
                test_->test_running ? 1 : 0,
                test_->refreshlist
                );
        if (test_->refreshlist != 0) {
            test_->refreshlist = 0; // reset refresh list command
        }

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

    static esp_err_t filedir_get_handler(httpd_req_t *req) {

        ESP_LOGI(TAG, "... FILEDIR REQUEST RECEIVED ...");
        ESP_LOGI(TAG, "%s", req->uri);

        std::ostringstream response_str;        

        const char* path = "/spiffs/data";
        DIR* dir = opendir(path);
        if (!dir) {
            ESP_LOGE(TAG, "Failed to open directory: %s", path);
            httpd_resp_send_500(req);
            return ESP_FAIL;            
        }

        response_str << "[";

        struct dirent* entry;
        bool first_item = true;
        while ((entry = readdir(dir)) != NULL) {
            // Skontrolujeme, či súbor má príponu ".csv"
            const char* ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".csv") == 0) {
                if (first_item) first_item = false;                    
                else            response_str << ",";

                double length, endvoltage;
                get_length_and_end_voltage(entry->d_name, length, endvoltage);
                response_str << "{";
                response_str << "\"filename\": \"" << entry->d_name << "\",";
                response_str << "\"length\": \"" << std::fixed << std::setprecision(1) << length << " sec\",";
                response_str << "\"endvoltage\": \"" << std::fixed << std::setprecision(2) << endvoltage << " V\"";
                response_str << "}";
           
                ESP_LOGI(TAG, "Found CSV file: %s", entry->d_name);
            }
        }
        response_str << "]";
        
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response_str.str().c_str(), response_str.str().length());
        
        return ESP_OK;
    };
    static constexpr httpd_uri_t filedir = {
        .uri       = "/filedir",
        .method    = HTTP_GET,
        .handler   = Webserver::filedir_get_handler,
        .user_ctx  = NULL
    };

    static bool get_length_and_end_voltage(char* filename, double &length, double &endvoltage){
        length = 0.0;
        endvoltage = 0.0;

        std::string path = "/spiffs/data/";
        path += filename;
                
        FILE *f = fopen(path.c_str(), "r");
        if (f == NULL) {        
            return false;
        }

        char line[128];

        // Skip the header line
        if (fgets(line, sizeof(line), f) == NULL) {
            fclose(f);
            return false;
        }

        while (fgets(line, sizeof(line), f)) {
            // Read from first two CSV columns: Time[s], Battery[V]
            if (sscanf(line, "%lf,%lf", &length, &endvoltage) == 2) {
                printf("length = %.3f, endvoltage = %.3f\n", length, endvoltage);
            } else {
                printf("Invalid line: %s\n", line);
            }
        }

        fclose(f);

        return true;
    }



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