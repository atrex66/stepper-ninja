#ifndef NINJA_PWM_H
#define NINJA_PWM_H
#include <stdint.h>

void ninja_pwm_init();
uint16_t pwm_calculate_wrap(uint32_t freq);
void ninja_pwm_set_frequency(uint32_t frequency);
void ninja_pwm_set_duty(uint16_t duty);

#endif