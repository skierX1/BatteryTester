#ifndef __VOLTAGE_H
#define __VOLTAGE_H

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
#include "esp_adc/adc_oneshot.h"
#include <esp_log.h>

#include "defines.h"


// GPIO to ADC channel mapping
typedef struct {
    gpio_num_t gpio;
    adc_channel_t adc_channel;
    const char* label;
} gpio_adc_mapping_t;

typedef struct {
    gpio_num_t gpio;                     // GPIO pin
    adc_channel_t adc_channel;          // ADC channel
    const char* label;                   // Label for identification
    adc_oneshot_unit_handle_t handle;    // ADC unit handle (driver_ng)
    adc_cali_handle_t cali_handle;       // Calibration handle
    bool calibrated;                     // Flag if calibration is available
} adc_channel_info_t;

const int NUM_GPIOs=3;

class VoltageReader {
    public:

    VoltageReader();
    ~VoltageReader();
    
    struct VoltageStruct {
      int    raw        = 0;
      bool   calibrated = false;
      double voltage    = 0.0f;
    };

    bool init_adc_calibration(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
    void setup_adc_channels();
    void read_adc_channel(int channel_index);
    void cleanup_adc_calibration();
    void ReadVoltage();
    
    // Note: GPIO1 is typically used for debug output
    // Note: GPIO2 is also used for boot mode in some cases
    const gpio_adc_mapping_t gpio_mappings[NUM_GPIOs] = {
        {GPIO_NUM_2, ADC_CHANNEL_2, "GPIO2"},  // battery cell 1
        {GPIO_NUM_3, ADC_CHANNEL_3, "GPIO3"},  // battery cell 2
        {GPIO_NUM_4, ADC_CHANNEL_4, "GPIO4"},  // battery overall voltage
    };    
    adc_channel_info_t adc_channels[NUM_GPIOs];

    VoltageStruct voltage[NUM_GPIOs];
};

#endif