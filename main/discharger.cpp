
#include "esp_log.h"
#include "esp_err.h"
#include "esp_mac.h"

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
#include "driver/adc.h"

#include "defines.h"

#include "discharger.h"

extern const char *TAG;
Discharger* discharger_;

Discharger::Discharger () {
    init_pwm_gpio41(1000, 0);   // 1 kHz PWM, 0% duty    
}

esp_err_t Discharger::init_pwm_gpio41(uint32_t freq_hz, uint16_t duty_percent) {
    
    // Configure LEDC timer
    ledc_timer_config_t timer_conf = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_10_BIT, // 0–1023
        .timer_num        = PWM_TIMER,
        .freq_hz          = freq_hz,
        .clk_cfg          = LEDC_AUTO_CLK
    };

    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) return ret;

    // Configure LEDC channel
    ledc_channel_config_t channel_conf = {
        .gpio_num       = 41,
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = PWM_CHANNEL,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = PWM_TIMER,
        .duty           = 0,   // start with 0% duty
        .hpoint         = 0,
        .flags          = {}
    };

    ret = ledc_channel_config(&channel_conf);
    if (ret != ESP_OK) return ret;

    // Apply initial duty
    return set_pwm_duty_gpio41(duty_percent);
}

esp_err_t Discharger::set_pwm_duty_gpio41(uint16_t duty_percent) {
    if (duty_percent > 100) duty_percent = 100;

    const uint32_t max_duty = (1 << 10) - 1; // 10-bit resolution → 0–1023
    uint32_t duty = (max_duty * duty_percent) / 100;

    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, duty);
    if (ret != ESP_OK) return ret;

    return ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL);
}