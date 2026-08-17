#ifndef __RC_CONTROL_H
#define __RC_CONTROL_H

#include "stdint.h"

typedef struct
{
  int16_t throttle;
  int16_t steering;
  uint8_t mode;
  uint8_t enable;
  uint8_t connected;
} RcCommand;

void rc_init(void);
void rc_update(void);
RcCommand rc_get_command(void);

/* Protocol adapter hook. A future PS2/SBUS/IBUS/CRSF/UART adapter overrides
   this weak function and returns 1 only when one complete fresh frame exists. */
uint8_t rc_backend_read(RcCommand *command);

#endif /* __RC_CONTROL_H */
