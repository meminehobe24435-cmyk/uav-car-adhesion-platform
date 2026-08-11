#include "bsp_iic.h"



/* 微秒延时函数（需用户根据主频实现，此处沿用原版） */
void Delay_us(uint32_t us)
{
    __IO uint32_t Delay = us * 72 / 8;   // 72MHz 主频，根据需要调整
    do { __NOP(); } while(Delay--);
}

#define IIC_Delay()  Delay_us(5)


// 微秒延时声明（需外部实现）
extern void Delay_us(uint32_t us);

// 设置SDA为输入模式（浮空）
void SDA_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = IIC_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(IIC_GPIO_PORT, &GPIO_InitStruct);
}

// 设置SDA为推挽输出模式
void SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = IIC_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(IIC_GPIO_PORT, &GPIO_InitStruct);
}

// IIC起始信号
static void IIC_Start(void)
{
    SDA_OUT();
    IIC_SDA_HIGH();
    IIC_SCL_HIGH();
    IIC_Delay();
    IIC_SDA_LOW();
    IIC_Delay();
    IIC_SCL_LOW();
}

// IIC停止信号
static void IIC_Stop(void)
{
    SDA_OUT();
    IIC_SCL_LOW();
    IIC_SDA_LOW();
    IIC_Delay();
    IIC_SCL_HIGH();
    IIC_SDA_HIGH();
    IIC_Delay();
}

// 等待从机应答
static uint8_t IIC_WaitACK(void)
{
    uint8_t errorTime = 0;
    SDA_IN();
    IIC_SDA_HIGH();   // 释放SDA总线
    IIC_Delay();
    IIC_SCL_HIGH();
    IIC_Delay();
    while(IIC_READ_SDA())
    {
        errorTime++;
        if(errorTime > 250)
        {
            IIC_Stop();
            return 1;
        }
    }
    IIC_SCL_LOW();
    return 0;
}

// 主机产生应答
static void IIC_Ack(void)
{
    IIC_SCL_LOW();
    SDA_OUT();
    IIC_SDA_LOW();
    IIC_Delay();
    IIC_SCL_HIGH();
    IIC_Delay();
    IIC_SCL_LOW();
}

// 主机产生非应答
static void IIC_NAck(void)
{
    IIC_SCL_LOW();
    SDA_OUT();
    IIC_SDA_HIGH();
    IIC_Delay();
    IIC_SCL_HIGH();
    IIC_Delay();
    IIC_SCL_LOW();
}

// 发送一个字节
static void IIC_SendByte(uint8_t Byte)
{
    uint8_t i;
    SDA_OUT();
    for(i = 0; i < 8; i++)
    {
        if(Byte & 0x80)
            IIC_SDA_HIGH();
        else
            IIC_SDA_LOW();
        Byte <<= 1;
        IIC_SCL_HIGH();
        IIC_Delay();
        IIC_SCL_LOW();
        IIC_Delay();
    }
}

// 读取一个字节，Ack=1时发送应答，否则发送非应答
static uint8_t IIC_ReadByte(uint8_t Ack)
{
    uint8_t i, Receive = 0;
    SDA_IN();
    for(i = 0; i < 8; i++)
    {
        IIC_SCL_LOW();
        IIC_Delay();
        IIC_SCL_HIGH();
        Receive <<= 1;
        if(IIC_READ_SDA())
            Receive++;
        IIC_Delay();
    }
    if(!Ack)
        IIC_NAck();
    else
        IIC_Ack();
    return Receive;
}

// 连续写寄存器
uint8_t IIC_SendBytes(uint8_t Dev, uint8_t RegAddr, uint8_t Len, uint8_t *Data)
{
    uint8_t i;
    IIC_Start();
    IIC_SendByte(Dev << 1);
    if(IIC_WaitACK()) return 0;
    IIC_SendByte(RegAddr);
    if(IIC_WaitACK()) return 0;
    for(i = 0; i < Len; i++)
    {
        IIC_SendByte(Data[i]);
        if(IIC_WaitACK()) return 0;
    }
    IIC_Stop();
    return 1;
}

// 连续读寄存器
uint8_t IIC_ReadBytes(uint8_t Dev, uint8_t RegAddr, uint8_t Len, uint8_t *Data)
{
    uint8_t Count;
    IIC_Start();
    IIC_SendByte(Dev << 1);
    if(IIC_WaitACK()) return 0;
    IIC_SendByte(RegAddr);
    if(IIC_WaitACK()) return 0;
    IIC_Start();
    IIC_SendByte((Dev << 1) | 0x01);
    if(IIC_WaitACK()) return 0;
    for(Count = 0; Count < Len; Count++)
    {
        if(Count != Len - 1)
            Data[Count] = IIC_ReadByte(1);
        else
            Data[Count] = IIC_ReadByte(0);
    }
    IIC_Stop();
    return Count;
}

// IIC初始化（由CubeMX完成GPIO配置，此函数可为空）
void IIC_Init(void)
{
    // CubeMX已初始化相应GPIO为推挽输出，默认SCL/SDA高电平
    // 如需额外操作，可在此添加
}


