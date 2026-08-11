#include "calc_fun.h"

int abs_int(int num)
{
    if (num < 0)
        return -num;  // 负数取反
    else
        return num;   // 正数直接返回
}

/**
  * 函    数：一阶低通滤波函数
  * 参    数：无
  * 返 回 值：无
  */
int16_t com_filter_lowpass(int16_t newData, int16_t lastData)
{
    return ALPHA * lastData + (1 - ALPHA) * newData;
}

// 辅助函数：short 转字节数组（小端）
void ShortToChar(short sData, unsigned char cData[])
{
    cData[0] = sData & 0xff;
    cData[1] = sData >> 8;
}

// 辅助函数：字节数组转 short（小端）
short CharToShort(unsigned char cData[])
{
    return ((short)cData[1] << 8) | cData[0];
}

