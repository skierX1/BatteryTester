
#include "voltage.h";
#include "driver/adc.h"
#include <esp_adc_cal.h>
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include <driver/adc.h>
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc_cal.h"

extern const char *TAG;
VoltageReader* voltage_reader_;

VoltageReader::VoltageReader() {
    setup_adc_channels();
}

VoltageReader::~VoltageReader() {
    cleanup_adc_calibration();
}

bool VoltageReader::init_adc_calibration(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Using curve fitting scheme for channel %d", channel);
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Using line fitting scheme for channel %d", channel);
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    return calibrated;
}

void VoltageReader::setup_adc_channels() {
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };

    esp_err_t ret = adc_oneshot_new_unit(&init_config1, &adc1_handle);
    if (ret == ESP_OK) {
        printf("ADC unit initialized successfully!\n");
    } else {
        printf("ADC init failed: %d\n", ret);
        return;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT        
    };

    for (int i = 0; i < NUM_GPIOs; i++) {
        // Configure channel using new API
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, gpio_mappings[i].adc_channel, &chan_cfg));

        // Initialize calibration (driver_ng compatible)
        adc_channels[i].calibrated = init_adc_calibration(
            ADC_UNIT_1,
            (adc_channel_t)gpio_mappings[i].adc_channel,
            ADC_ATTEN_DB_11,
            &adc_channels[i].cali_handle
        );

        adc_channels[i].handle = adc1_handle;
        adc_channels[i].label = gpio_mappings[i].label;
        adc_channels[i].adc_channel = gpio_mappings[i].adc_channel;

        if (adc_channels[i].calibrated) {
            ESP_LOGI(TAG, "%s calibrated successfully", gpio_mappings[i].label);
        } else {
            ESP_LOGW(TAG, "%s using approximate voltage calculation", gpio_mappings[i].label);
        }
    }
}



void VoltageReader::read_adc_channel(int channel_index) {
    // Read multiple samples for better accuracy
    uint32_t adc_reading = 0;
    const int samples = 64;

    for (int i = 0; i < samples; i++) {
        //adc_reading += adc1_get_raw(adc_channels[channel_index].channel);

         int raw = 0;
        // Use driver_ng API for reading
        ESP_ERROR_CHECK(adc_oneshot_read(adc_channels[channel_index].handle,
                                         adc_channels[channel_index].adc_channel,
                                         &raw));
        adc_reading += raw;

    }
    adc_reading /= samples;
    voltage[channel_index].raw = adc_reading;
    voltage[channel_index].calibrated = adc_channels[channel_index].calibrated;
    
    // Convert to voltage    
    if (adc_channels[channel_index].calibrated) {
        int voltage_mv = 0;
        if (adc_cali_raw_to_voltage(adc_channels[channel_index].cali_handle, adc_reading, &voltage_mv) == ESP_OK) {
            voltage[channel_index].voltage = voltage_mv / 1000.0f;
            return;
        }
    }
    else {
        // Fallback: approximate calculation for ADC_ATTEN_DB_12 (approx 3.3V full scale)    
        voltage[channel_index].voltage = (adc_reading * 3.3) / 4095.0;; // not used, not tested, probably incorrect
    }
}

void VoltageReader::cleanup_adc_calibration() {
    for (int i = 0; i < NUM_GPIOs; i++) {
        if (adc_channels[i].calibrated) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
            adc_cali_delete_scheme_curve_fitting(adc_channels[i].cali_handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
            adc_cali_delete_scheme_line_fitting(adc_channels[i].cali_handle);
#endif
        }
    }
}

void VoltageReader::ReadVoltage() {
    for (int i = 0; i < NUM_GPIOs; i++) {        
        read_adc_channel(i);            
    }
}