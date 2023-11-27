#ifndef SBITX_GPIO_H_
#define SBITX_GPIO_H_

#include "sbitx_core.h"

void gpio_init(radio *radio_h);

// callback functions
void tuning_isr_a(void);
void tuning_isr_b(void);
void knob_a_pressed(void);
void knob_b_pressed(void);
void ptt_change(void);
void dash_change(void);

// encoder-related functions
void enc_init(encoder *e, int speed, int pin_a, int pin_b);
int enc_state (encoder *e);
int enc_read(encoder *e);


#endif // SBITX_GPIO_H_
