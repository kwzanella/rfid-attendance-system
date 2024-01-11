#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
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

#define RESPONSE_SUCCESS_BIT  BIT0
#define RESPONSE_FAIL_BIT     BIT1

static const char *TAG = "app_main";
QueueHandle_t mqtt_in;
QueueHandle_t mqtt_out;
static EventGroupHandle_t s_response_event_group;

MFRC522DriverPinSimple chip_select(CS_PIN);
MFRC522DriverSPI driver{chip_select};   
MFRC522 mfrc522{driver};

uint8_t s_led_state = 0;

void configure_led(void) {
    // TODO: proper GPIO config and init
    gpio_reset_pin(RED_LED);
    gpio_set_direction(RED_LED, GPIO_MODE_OUTPUT);
    gpio_reset_pin(GREEN_LED);
    gpio_set_direction(GREEN_LED, GPIO_MODE_OUTPUT);
}

void hardware_action_task(void *pvParameters) {
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(s_response_event_group,
            RESPONSE_SUCCESS_BIT | RESPONSE_FAIL_BIT,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY);

        if (bits & RESPONSE_SUCCESS_BIT) {
            printf("SUCCESS\n");
            gpio_set_level(GREEN_LED, 1);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            gpio_set_level(GREEN_LED, 0);

            play_tone(SUCCESS_FREQUENCY, TONE_DURATION);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        else if (bits & RESPONSE_FAIL_BIT) {
            printf("FAILURE\n");
            gpio_set_level(RED_LED, 1);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            gpio_set_level(RED_LED, 0);

            play_tone(FAILURE_FREQUENCY, TONE_DURATION);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
}

void byte_array_to_str(byte array[], uint8_t len, char buffer[]) {
    for (uint8_t i = 0; i < len; ++i) {
        // Extract the upper and lower 4 bits of each byte in the array
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;

        // Convert the 4-bit values to ASCII characters
        buffer[i * 2 + 0] = nib1 < 0xA ? '0' + nib1 : 'A' + nib1 - 0xA;
        buffer[i * 2 + 1] = nib2 < 0xA ? '0' + nib2 : 'A' + nib2 - 0xA;
    }
    buffer[len * 2] = '\0';  // Add a null terminator at the end of the string
}

void mfrc522_task(void *pvParameters) {
    for(;;) {
        // Reseta o loop se não tiver cartão novo presente no reader
        if ( ! mfrc522.PICC_IsNewCardPresent() || ! mfrc522.PICC_ReadCardSerial()) {
            continue;
        }
        char uid_str[mfrc522.uid.size *2 + 1] = "";
        byte_array_to_str(mfrc522.uid.uidByte, mfrc522.uid.size, uid_str);
        
        xQueueSend(mqtt_out, &uid_str, 0);
        mfrc522.PICC_HaltA();
    }
    vTaskDelete(NULL);
}

void UID_publish_task(void *pvParameters) {
    for(;;) {
        char uid_str[9] = "";
        uid_str[8] = '\0';  // Null-terminate the buffer just to be sure
        xQueueReceive(mqtt_out, uid_str, portMAX_DELAY);  // HACK: does portMAX_DELAY actually wait indefinetely?

        ESP_LOGI(TAG, "PICC UID: %s", uid_str);
        publish("topic/qos1", uid_str, 0, 1, 0);
    }
    vTaskDelete(NULL);
}

void response_task(void *pvParameters) {
    for(;;) {
        char buffer[2];  // Buffer to store "0" or "1"
        buffer[1] = '\0';  // Null-terminate the buffer just to be sure

        xQueueReceive(mqtt_in, buffer, portMAX_DELAY);
        printf("Response: %s\n", buffer);

        if(strcmp(buffer, "1") == 0) {
            xEventGroupSetBits(s_response_event_group, RESPONSE_SUCCESS_BIT);
        }
        else if(strcmp(buffer, "0") == 0) {
            xEventGroupSetBits(s_response_event_group, RESPONSE_FAIL_BIT);
        }
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

    mqtt_in = xQueueCreate( 5, sizeof(char) );  // TODO: delete after use
    mqtt_out = xQueueCreate( 5, 8 * sizeof(char) );  // TODO: delete after use
	configASSERT( mqtt_in );
    configASSERT( mqtt_out );

    mqtt_app_start();
    configure_led();

    buzzer_init();
    mfrc522.PCD_Init();

    s_response_event_group = xEventGroupCreate();  // TODO: proper deletion

    // TODO: define proper parameters
    xTaskCreate(&mfrc522_task, "mfrc522_task", 2048, NULL, 5, NULL);
    xTaskCreate(&response_task, "comm_task", 2048, NULL, 5, NULL);
    xTaskCreate(&UID_publish_task, "mfrc522_task", 2048, NULL, 5, NULL);
    xTaskCreate(&hardware_action_task, "hardware_action_task", 2048, NULL, 5, NULL);
}