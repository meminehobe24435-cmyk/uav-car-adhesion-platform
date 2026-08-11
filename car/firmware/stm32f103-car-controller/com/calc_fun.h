#ifndef __CALC_FUN_H
#define __CALC_FUN_H

#include "stdint.h"

#define ALPHA 0.7

extern int abs_int(int num);
extern int16_t com_filter_lowpass(int16_t newData, int16_t lastData);
void ShortToChar(short sData, unsigned char cData[]);

// 辅助函数：字节数组转 short（小端）
short CharToShort(unsigned char cData[]);



#endif

