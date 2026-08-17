#ifndef __FLIGHT_COMM_H
#define __FLIGHT_COMM_H

#include "stdint.h"

/* Set to 0 before linking a different strong rc_backend_read() adapter. */
#define FLIGHT_COMM_PROVIDE_RC_BACKEND 1

/*
 * Phase-1 UART loopback safety gate.
 * Keep this at 0 while validating COMTool <-> USART2 diagnostics.  A valid
 * frame is still parsed and acknowledged, but rc_control only receives a
 * disabled, zero-speed command so no motor action can occur.
 */
#define FLIGHT_COMM_ENABLE_CAR_OUTPUT 0

typedef struct
{
  int16_t throttle;
  int16_t steering;
  uint8_t mode;
  uint8_t enable;
  uint8_t connected;
} FlightCarCommand;

void flight_comm_init(void);
void flight_comm_update(void);
FlightCarCommand flight_comm_get_command(void);
uint8_t flight_comm_is_connected(void);

#endif /* __FLIGHT_COMM_H */
