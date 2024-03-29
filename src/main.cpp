#include <stdio.h>
#include <assert.h>

#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "Arduino.h"
#include "MFRC522v2.h"
#include "MFRC522DriverSPI.h"
#include "MFRC522DriverPinSimple.h"
#include "MFRC522Debug.h"

#include "secrets.h"
#include "wifi_handler.h"
#include "mqtt_handler.h"
#include "buzzer_controller.h"

/*
* this code follows the ESP-IDF style guide defined in:
* https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style-guide.html
*/


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

#define COPI_PIN              GPIO_NUM_23
#define CIPO_PIN              GPIO_NUM_19
#define CS_PIN                GPIO_NUM_5 
#define RST_PIN               GPIO_NUM_15
#define SCK_PIN               GPIO_NUM_18

#define GREEN_LED             GPIO_NUM_21
#define RED_LED               GPIO_NUM_4

#define LED_DELAY             200
#define TONE_DURATION         200
#define ACTION_DELAY          500

#define SUCCESS_FREQUENCY     1000
#define FAILURE_FREQUENCY     500

// used for event group control
#define RESPONSE_SUCCESS_BIT  BIT0
#define RESPONSE_FAIL_BIT     BIT1

static const char *TAG = "app_main";

QueueHandle_t sub_queue;  // queue to store data from SUB_TOPIC
QueueHandle_t pub_queue;  // queue to store data from PUB_TOPIC

static EventGroupHandle_t s_hardware_event_group;  // event group to control hardware accordingly

// both redundant, only used to init MFRC522 class
static MFRC522DriverPinSimple s_chip_select(CS_PIN);
static MFRC522DriverSPI s_driver{s_chip_select};

static MFRC522 s_mfrc522{s_driver};

// TODO: proper GPIO config and init. Will remove this function later
void configure_led(void)
{
    gpio_reset_pin(RED_LED);
    gpio_set_direction(RED_LED, GPIO_MODE_OUTPUT);
    gpio_reset_pin(GREEN_LED);
    gpio_set_direction(GREEN_LED, GPIO_MODE_OUTPUT);
}

// Converts byte array to C string 
void byte_array_to_str(byte array[], uint8_t len, char buffer[])
{
    for (uint8_t i = 0; i < len; ++i) {
        // extract the upper and lower 4 bits of each byte in the array
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;

        // convert the 4-bit values to ASCII characters
        buffer[i * 2 + 0] = nib1 < 0xA ? '0' + nib1 : 'A' + nib1 - 0xA;
        buffer[i * 2 + 1] = nib2 < 0xA ? '0' + nib2 : 'A' + nib2 - 0xA;
    }
    buffer[len * 2] = '\0';
}

// Controls the LED and buzzer according to the data from the event group
void hardware_action_task(void *pvParameters)
{
    for (;;) {
        EventBits_t bits = xEventGroupWaitBits(s_hardware_event_group,
            RESPONSE_SUCCESS_BIT | RESPONSE_FAIL_BIT,
            pdTRUE,  // reset bits after exit
            pdFALSE,
            portMAX_DELAY);

        if (bits & RESPONSE_SUCCESS_BIT) {
            gpio_set_level(GREEN_LED, 1);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            gpio_set_level(GREEN_LED, 0);

            play_tone(SUCCESS_FREQUENCY, TONE_DURATION);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        } else if (bits & RESPONSE_FAIL_BIT) {
            gpio_set_level(RED_LED, 1);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            gpio_set_level(RED_LED, 0);

            play_tone(FAILURE_FREQUENCY, TONE_DURATION);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
}

// Collects the UID for publishing
void mfrc522_task(void *pvParameters)
{
    for (;;) {
        if ( ! s_mfrc522.PICC_IsNewCardPresent() || ! s_mfrc522.PICC_ReadCardSerial() ) {
            continue;
        }
        char uid_str[s_mfrc522.uid.size *2 + 1] = "";
        byte_array_to_str(s_mfrc522.uid.uidByte, s_mfrc522.uid.size, uid_str);
        
        xQueueSend(pub_queue, &uid_str, 0);
        s_mfrc522.PICC_HaltA();
    }
    vTaskDelete(NULL);
}

// Publishes UID to PUB_TOPIC
void UID_publish_task(void *pvParameters)
{
    for (;;) {
        char uid_str[9] = "";
        uid_str[8] = '\0';
        xQueueReceive(pub_queue, uid_str, portMAX_DELAY);  // wait until data is received

        ESP_LOGI(TAG, "PICC UID: %s", uid_str);
        publish(PUB_TOPIC, uid_str, 0, 1, 0);
    }
    vTaskDelete(NULL);
}

// Receives response from the server and sets according event group bits
void response_task(void *pvParameters)
{
    for (;;) {
        char buffer[2] = "";
        buffer[1] = '\0';  // null-terminate the buffer just to be sure

        xQueueReceive(sub_queue, buffer, portMAX_DELAY);  // wait until data is received
        ESP_LOGI(TAG, "Server Response: %s", buffer);

        if(strcmp(buffer, "1") == 0) {
            xEventGroupSetBits(s_hardware_event_group, RESPONSE_SUCCESS_BIT);
        } else if(strcmp(buffer, "0") == 0) {
            xEventGroupSetBits(s_hardware_event_group, RESPONSE_FAIL_BIT);
        }
    }
    vTaskDelete(NULL);
}

extern "C" void app_main()
{
    initArduino();

    wifi_app_start();  // TODO: Treat return code

    mqtt_app_start();  // TODO: Adapt function to verify if started correctly

    // Does not need to be deleted as it will last the entire program
    sub_queue = xQueueCreate(5, sizeof(char));
    pub_queue = xQueueCreate(5, 8 * sizeof(char));
    s_hardware_event_group = xEventGroupCreate();

    s_mfrc522.PCD_Init();
    configure_led();
    buzzer_init();

    xTaskCreate(&mfrc522_task,         "mfrc522_task",         configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(&hardware_action_task, "hardware_action_task", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(&response_task,        "response_task",        configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, NULL);
    xTaskCreate(&UID_publish_task,     "mfrc522_task",         configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 2, NULL);
}