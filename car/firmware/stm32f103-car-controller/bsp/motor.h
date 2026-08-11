#ifndef __MOTOR_H
#define __MOTOR_H

#include "stdint.h"
#include "chassis_mode.h"

/* Encoder parameters retained for the later closed-loop stage. */
#define ENC_LINE            11.0f
#define ENC_MULTIPLE         4.0f
#define GEAR_RATIO          30.0f
#define WHEEL_DIAMETER       0.065f
#define CAPTURE_PERIOD       0.02f
#define PI                   3.1415926f

/* MOTOR_1..4 map directly to J1..J4 until wheel positions are confirmed. */
typedef enum
{
  MOTOR_1 = 0,
  MOTOR_2,
  MOTOR_3,
  MOTOR_4,
  MOTOR_COUNT
} MotorId;

typedef struct
{
  int8_t direction;
  uint16_t pwm_max;
} MotorConfig;

extern MotorConfig motor_config[MOTOR_COUNT];

void motor_init(void);
void motor_set_pwm(MotorId id, int16_t pwm);
void motor_forward(MotorId id, uint16_t pwm);
void motor_reverse(MotorId id, uint16_t pwm);
void motor_stop(MotorId id);
void motor_stop_all(void);
void motor_set_all(int16_t motor_1,
                   int16_t motor_2,
                   int16_t motor_3,
                   int16_t motor_4);

uint16_t motor_get_pwm_full_scale(void);
uint16_t motor_percent_to_pwm(uint16_t percent);

void motor_single_test(void);
void motor_four_test(void);

void car_stop(void);
void car_forward(uint16_t pwm);
void car_backward(uint16_t pwm);

/* Compatibility entry point retained for the existing, inactive chassis code. */
void motor_ctrol(Chassis_TypeDef *chassis);

#endif /* __MOTOR_H */
