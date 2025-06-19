#ifndef NINJA_PWM_H
#define NINJA_PWM_H
#include <stdint.h>
#include "config.h"

void ninja_pwm_init(uint8_t pin);
uint16_t pwm_calculate_wrap(uint32_t freq);
void ninja_pwm_set_frequency(uint8_t pin, uint32_t frequency);
void ninja_pwm_set_duty(uint8_t pin, uint16_t duty);

#endif