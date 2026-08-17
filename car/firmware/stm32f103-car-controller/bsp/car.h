#ifndef __CAR_H
#define __CAR_H

#include "stdint.h"

void car_init(void);
void car_stop(void);
void car_forward(uint16_t speed);
void car_backward(uint16_t speed);
void car_turn(float steering_angle, int16_t speed);
void car_crab(float steering_angle, int16_t speed);

#endif /* __CAR_H */
