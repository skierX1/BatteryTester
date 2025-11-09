#ifndef __FILES_RW
#define __FILES_RW

#include <stdio.h>
#include <string>

#include "defines.h"

extern const char *TAG;

class Spiffs {
public:
    Spiffs(){};
    ~Spiffs(){};

    char* Read(FILE *f);

    static FILE* Open(char* filename){
        // Try opening the file you added        

        char file_name[10+strlen(filename)];
        sprintf(file_name,"/spiffs/%s", filename);
        FILE *f = fopen(file_name, "r");
        
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for reading");    
        }

        return f;
    };
    
    static void Close(FILE *f) {
        fclose(f);
    }
    
    static bool Activate() {
            esp_vfs_spiffs_conf_t conf = {
            .base_path = "/spiffs",
            .partition_label = NULL,
            .max_files = 5,
            .format_if_mount_failed = true
        };

        esp_err_t ret = esp_vfs_spiffs_register(&conf);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mount or format SPIFFS (%s)", esp_err_to_name(ret));
            return false;
        }

        size_t total = 0, used = 0;
        ret = esp_spiffs_info(NULL, &total, &used);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "SPIFFS total: %d bytes, used: %d bytes", total, used);
        }    

        return true;
    };
    static void Deactivate(){
        esp_vfs_spiffs_unregister(NULL);
        ESP_LOGI(TAG, "SPIFFS unmounted");
    }


private:
    std::string text;
    
};

#endif //__FILES_RW