#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "secrets.h"
#include "wifi_handler.h"
#include "mqtt_handler.h"
#include "buzzer_controller.h"

#include "Arduino.h"
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>


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

#define GREEN_LED          GPIO_NUM_21
#define RED_LED            GPIO_NUM_4

#define LED_DELAY          200
#define TONE_DURATION      200
#define ACTION_DELAY       500

#define SUCCESS_FREQUENCY  1000
#define FAILURE_FREQUENCY  500

static const char *TAG = "app_main";

MFRC522DriverPinSimple chip_select(CS_PIN);
MFRC522DriverSPI driver{chip_select};   
MFRC522 mfrc522{driver};

uint8_t s_led_state = 0;

void configure_led(void) {
    // TODO: proper GPIO config and init
    gpio_reset_pin(RED_LED);
    gpio_set_direction(RED_LED, GPIO_MODE_OUTPUT);
}

void blink_led(void) {
    gpio_set_level(RED_LED, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(RED_LED, 0);
}


void blink_led_task(void *pvParameters) {
    while (1) {
        blink_led();
        play_tone(SUCCESS_FREQUENCY, 1000);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void print_hex(const byte* byte_array, const uint8_t length) {
    for (byte i = 0; i < length; ++i) {
        printf("%x ", byte_array[i]);
    }
    printf("\n");
}

void mfrc522_task(void *pvParameters) {
    for(;;) {
        // Reseta o loop se não tiver cartão novo presente no reader
        if ( ! mfrc522.PICC_IsNewCardPresent() || ! mfrc522.PICC_ReadCardSerial()) {
            continue;
        }
        printf("PICC UID: ");  // PICC = Proximity Integrated Circuit Card
        print_hex(mfrc522.uid.uidByte, mfrc522.uid.size);
        mfrc522.PICC_HaltA();
    }
    vTaskDelete(NULL);
}

// Meant to initialize and start main functionality
extern "C" void app_main() {
    initArduino();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();   
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    if (wifi_init_sta() != ESP_OK) {  // only continues if wifi is initialized correctly
        return;
    }

    mqtt_app_start();
    configure_led();

    buzzer_init();
    mfrc522.PCD_Init();

    xTaskCreate(&blink_led_task, "blink_led_task", 2048, NULL, 5, NULL);
    xTaskCreate(&mfrc522_task, "mfrc522_task", 2048, NULL, 5, NULL);
}