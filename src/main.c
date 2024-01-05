#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"

#include "secrets.h"

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
#define BUZZER_DUTY        (uint16_t)  8191  // Determines the volume. Max value is 2**LEDC_TIMER_13_BIT

#define LED_DELAY          (uint16_t)  200
#define TONE_DURATION      (uint16_t)  200
#define ACTION_DELAY       (uint16_t)  500

#define SUCCESS_FREQUENCY  (uint32_t)  1000
#define FAILURE_FREQUENCY  (uint32_t)  500

#define WIFI_MAX_RETRY 5

static const char *TAG = "wifi station";
static int s_retry_num = 0;
uint8_t s_led_state = 0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {  // start connection
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {  // wifi failed
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else {
            ESP_LOGI(TAG, "Failed to connect to AP");
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {  // wifi connected
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;

        ESP_LOGI(TAG, "connected to AP");
    }
}

void wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

void configure_led(void) {
    gpio_reset_pin(RED_LED);
    gpio_set_direction(RED_LED, GPIO_MODE_OUTPUT);
}

void blink_led(void) {
    gpio_set_level(RED_LED, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(RED_LED, 0);
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

void blink_led_task(void *pvParameters) {
    while (1) {
        blink_led();
        play_tone(SUCCESS_FREQUENCY, 1000);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// Meant to initialize and start main functionality
void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    configure_led();
    init_buzzer();
    xTaskCreate(&blink_led_task, "blink_led_task", 2048, NULL, 5, NULL);
}