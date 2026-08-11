#ifndef __SERVO_H
#define __SERVO_H

#include "stm32f1xx_hal.h"

typedef enum
{
  SERVO_LF = 0,
  SERVO_RF,
  SERVO_LR,
  SERVO_RR,
  SERVO_COUNT
} ServoId;

typedef struct
{
  uint16_t center_us;
  uint16_t min_us;
  uint16_t max_us;
  int8_t direction;
} ServoConfig;

/* Central calibration table. Edit center_us and direction here as needed. */
extern ServoConfig servo_config[SERVO_COUNT];

void servo_init(void);
void servo_set_us(ServoId id, uint16_t us);
void servo_center(ServoId id);
void servo_center_all(void);
void servo_set_center(ServoId id, uint16_t center_us);
void servo_set_direction(ServoId id, int8_t direction);
void servo_set_angle(ServoId id, float angle_deg);
void servo_set_all(uint16_t lf,
                   uint16_t rf,
                   uint16_t lr,
                   uint16_t rr);
void servo_calibration_test(void);

/* Called only by TIM5_IRQHandler. */
void servo_tim5_irq_handler(void);

#endif /* __SERVO_H */
