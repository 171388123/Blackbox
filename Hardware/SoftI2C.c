#include "SoftI2C.h"

#define SOFTI2C_PORT        GPIOB
#define SOFTI2C_SCL_PIN     GPIO_Pin_8
#define SOFTI2C_SDA_PIN     GPIO_Pin_9

#define SOFTI2C_W_SCL(x)    GPIO_WriteBit(SOFTI2C_PORT, SOFTI2C_SCL_PIN, (BitAction)(x))
#define SOFTI2C_W_SDA(x)    GPIO_WriteBit(SOFTI2C_PORT, SOFTI2C_SDA_PIN, (BitAction)(x))
#define SOFTI2C_R_SDA()     GPIO_ReadInputDataBit(SOFTI2C_PORT, SOFTI2C_SDA_PIN)

static void SoftI2C_Delay(void)
{
    uint8_t i;
    for (i = 0; i < 10; i++);
}

static void SoftI2C_SDA_Mode_Out(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = SOFTI2C_SDA_PIN;
    GPIO_Init(SOFTI2C_PORT, &GPIO_InitStructure);
}

static void SoftI2C_SDA_Mode_In(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = SOFTI2C_SDA_PIN;
    GPIO_Init(SOFTI2C_PORT, &GPIO_InitStructure);
}

void SoftI2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_InitStructure.GPIO_Pin = SOFTI2C_SCL_PIN;
    GPIO_Init(SOFTI2C_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = SOFTI2C_SDA_PIN;
    GPIO_Init(SOFTI2C_PORT, &GPIO_InitStructure);

    SOFTI2C_W_SCL(1);
    SOFTI2C_W_SDA(1);
}

void SoftI2C_Start(void)
{
    SoftI2C_SDA_Mode_Out();

    SOFTI2C_W_SDA(1);
    SOFTI2C_W_SCL(1);
    SoftI2C_Delay();

    SOFTI2C_W_SDA(0);
    SoftI2C_Delay();

    SOFTI2C_W_SCL(0);
    SoftI2C_Delay();
}

void SoftI2C_Stop(void)
{
    SoftI2C_SDA_Mode_Out();

    SOFTI2C_W_SDA(0);
    SoftI2C_Delay();

    SOFTI2C_W_SCL(1);
    SoftI2C_Delay();

    SOFTI2C_W_SDA(1);
    SoftI2C_Delay();
}

uint8_t SoftI2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    uint8_t AckBit;

    SoftI2C_SDA_Mode_Out();

    for (i = 0; i < 8; i++)
    {
        SOFTI2C_W_SDA((Byte & 0x80) ? 1 : 0);
        SoftI2C_Delay();

        SOFTI2C_W_SCL(1);
        SoftI2C_Delay();

        SOFTI2C_W_SCL(0);
        SoftI2C_Delay();

        Byte <<= 1;
    }

    SoftI2C_SDA_Mode_In();
    SOFTI2C_W_SDA(1);
    SoftI2C_Delay();

    SOFTI2C_W_SCL(1);
    SoftI2C_Delay();

    AckBit = SOFTI2C_R_SDA();

    SOFTI2C_W_SCL(0);
    SoftI2C_Delay();

    SoftI2C_SDA_Mode_Out();

    return AckBit;   // 0 = ACK, 1 = NACK
}

uint8_t SoftI2C_ReceiveByte(void)
{
    uint8_t i;
    uint8_t Byte = 0;

    SoftI2C_SDA_Mode_In();

    for (i = 0; i < 8; i++)
    {
        SOFTI2C_W_SCL(1);
        SoftI2C_Delay();

        if (SOFTI2C_R_SDA())
        {
            Byte |= (0x80 >> i);
        }

        SOFTI2C_W_SCL(0);
        SoftI2C_Delay();
    }

    SoftI2C_SDA_Mode_Out();

    return Byte;
}

void SoftI2C_SendAck(uint8_t AckBit)
{
    SoftI2C_SDA_Mode_Out();

    SOFTI2C_W_SDA(AckBit);
    SoftI2C_Delay();

    SOFTI2C_W_SCL(1);
    SoftI2C_Delay();

    SOFTI2C_W_SCL(0);
    SoftI2C_Delay();

    SOFTI2C_W_SDA(1);
}

uint8_t SoftI2C_WriteReg(uint8_t DevAddr, uint8_t RegAddr, uint8_t Data)
{
    SoftI2C_Start();

    if (SoftI2C_SendByte(DevAddr)) { SoftI2C_Stop(); return 1; }
    if (SoftI2C_SendByte(RegAddr)) { SoftI2C_Stop(); return 2; }
    if (SoftI2C_SendByte(Data))    { SoftI2C_Stop(); return 3; }

    SoftI2C_Stop();
    return 0;
}

uint8_t SoftI2C_ReadReg(uint8_t DevAddr, uint8_t RegAddr)
{
    uint8_t Data;

    SoftI2C_Start();
    SoftI2C_SendByte(DevAddr);
    SoftI2C_SendByte(RegAddr);

    SoftI2C_Start();
    SoftI2C_SendByte(DevAddr | 0x01);

    Data = SoftI2C_ReceiveByte();
    SoftI2C_SendAck(1);    // 最后一个字节发送NACK
    SoftI2C_Stop();

    return Data;
}

void SoftI2C_ReadRegs(uint8_t DevAddr, uint8_t RegAddr, uint8_t *DataArray, uint8_t Length)
{
    uint8_t i;

    SoftI2C_Start();
    SoftI2C_SendByte(DevAddr);
    SoftI2C_SendByte(RegAddr);

    SoftI2C_Start();
    SoftI2C_SendByte(DevAddr | 0x01);

    for (i = 0; i < Length; i++)
    {
        DataArray[i] = SoftI2C_ReceiveByte();

        if (i == Length - 1)
        {
            SoftI2C_SendAck(1);    // 最后一个字节 NACK
        }
        else
        {
            SoftI2C_SendAck(0);    // 前面的字节 ACK
        }
    }

    SoftI2C_Stop();
}