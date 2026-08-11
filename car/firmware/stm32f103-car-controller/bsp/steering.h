#ifndef __STEERING_H
#define __STEERING_H

void steering_center(void);
void steering_crab(float angle_deg);
void steering_turn(float angle_deg);
void steering_test_angle(float angle_deg);

/* Standalone test modes. Each function intentionally runs forever. */
void steering_direction_calibration_test(void);
void steering_safe_angle_test(void);
void steering_safe_angle_20_test(void);
void steering_basic_test(void);

#endif /* __STEERING_H */
