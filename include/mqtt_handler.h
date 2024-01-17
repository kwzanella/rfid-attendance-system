#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_event.h"

void log_error_if_nonzero(const char *message, int error_code);
void mqtt_app_start(void);
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
void publish(const char *topic, const char *data, int len, int qos, int retain);

#ifdef __cplusplus
}
#endif
