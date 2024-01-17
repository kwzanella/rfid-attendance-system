#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_event.h"

esp_err_t wifi_init_sta(void);
void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);

#ifdef __cplusplus
}
#endif
