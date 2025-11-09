idf_component_register(SRCS "webserver.cpp" "files_rw.cpp" "main.cpp" "voltage.cpp"
                       PRIV_REQUIRES spi_flash spiffs driver esp_adc esp_http_server esp_wifi wpa_supplicant nvs_flash                       
                       INCLUDE_DIRS ".")
