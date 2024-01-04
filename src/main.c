#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
/*
 * -------------------------------------------
 *              RFID-RC522      ESP-WROOM-32
 * Signal       Pin             Pin
 * -------------------------------------------
 * SPI CS       NSS             GPIO 5
 * SPI COPI     MOSI            GPIO 23
 * SPI CIPO     MISO            GPIO 19
 * SPI SCK      SCK             GPIO 18
 * Reset        RST             GPIO 15
 * Red LED                      GPIO 4
 * Green LED                    GPIO 21
 * Buzzer                       GPIO 22
*/

#define COPI_PIN           GPIO_NUM_23
#define CIPO_PIN           GPIO_NUM_19
#define CS_PIN             GPIO_NUM_5 
#define RST_PIN            GPIO_NUM_15
#define SCK_PIN            GPIO_NUM_18

#define BUZZER_PIN         GPIO_NUM_22
#define GREEN_LED          GPIO_NUM_21
#define RED_LED            GPIO_NUM_4

#define BUZZER_CHANNEL     LEDC_CHANNEL_0
#define DUTY_RESOLUTION    LEDC_TIMER_13_BIT

#define LED_DELAY          (uint16_t)  200

#define TONE_DURATION      (uint16_t)  200
#define ACTION_DELAY       (uint16_t)  500

#define SUCCESS_FREQUENCY  (uint32_t)  1000
#define FAILURE_FREQUENCY  (uint32_t)  500
#define BUZZER_DUTY        (uint16_t)  8191  // Determines the volume. Max value is 2**LEDC_TIMER_13_BIT



uint8_t s_led_state = 0;

void configure_led(void) {
    gpio_reset_pin(RED_LED);
    gpio_set_direction(RED_LED, GPIO_MODE_OUTPUT);
}

void blink_led(void) {
    gpio_set_level(RED_LED, s_led_state);
}

void init_buzzer() {
    // Configure timer and channel for the buzzer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = DUTY_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = SUCCESS_FREQUENCY,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = BUZZER_PIN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = BUZZER_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
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

void app_main() {

    while (1) {
        blink_led();
        s_led_state = !s_led_state;
        play_tone(SUCCESS_FREQUENCY, 1000);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}