#ifndef BUZZER_CONTROLLER_H
#define BUZZER_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void buzzer_init(void);
void play_tone(const uint32_t frequency, const uint16_t duration);

#ifdef __cplusplus
}
#endif

#endif  // BUZZER_CONTROLLER_H