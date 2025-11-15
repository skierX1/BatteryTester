#ifndef __DISCHARGER_H
#define __DISCHARGER_H

#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_err.h"

    

class Discharger {
public :
    
    static const ledc_channel_t PWM_CHANNEL = LEDC_CHANNEL_0;
    static const ledc_timer_t   PWM_TIMER   = LEDC_TIMER_0;

    Discharger();
    ~Discharger() {};
    
    esp_err_t set_pwm_duty_gpio41(uint16_t duty_percent);    
    esp_err_t init_pwm_gpio41(uint32_t freq_hz, uint16_t duty_percent);
};

#endif //__DISCHARGER_H