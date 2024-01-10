#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

#include "buzzer_controller.h"

#define BUZZER_PIN         GPIO_NUM_22
#define BUZZER_CHANNEL     LEDC_CHANNEL_0
#define DUTY_RESOLUTION    LEDC_TIMER_13_BIT
#define BUZZER_DUTY        8191  // Determines the volume. Max value is 2**LEDC_TIMER_13_BIT

void buzzer_init() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = DUTY_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 1000,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = BUZZER_PIN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = BUZZER_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        /* 
        * IMPORTANT!!!!
        * hpoint has to be set to something or the buzzer will have undefined behavior
        * Refer to the LEDC API reference and "LED PWM Controller (LEDC)" chapter in the ESP32 technical reference
        */
        .hpoint = 8190,
    };
    ledc_channel_config(&ledc_channel);
}


void play_tone(const uint32_t frequency, const uint16_t duration) {
    ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, frequency);

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, BUZZER_CHANNEL, BUZZER_DUTY);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, BUZZER_CHANNEL);

    vTaskDelay(duration / portTICK_PERIOD_MS);

    ledc_set_duty(LEDC_HIGH_SPEED_MODE, BUZZER_CHANNEL, 0);  // Turns the buzzer off
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, BUZZER_CHANNEL);
}