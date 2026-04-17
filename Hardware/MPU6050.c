#include "MPU6050.h"
#include "SoftI2C.h"

#define MPU6050_ADDRESS         0xD0

#define MPU6050_SMPLRT_DIV      0x19
#define MPU6050_CONFIG          0x1A
#define MPU6050_GYRO_CONFIG     0x1B
#define MPU6050_ACCEL_CONFIG    0x1C

#define MPU6050_ACCEL_XOUT_H    0x3B
#define MPU6050_PWR_MGMT_1      0x6B
#define MPU6050_PWR_MGMT_2      0x6C
#define MPU6050_WHO_AM_I        0x75

static void MPU6050_WriteReg(uint8_t RegAddr, uint8_t Data)
{
    SoftI2C_WriteReg(MPU6050_ADDRESS, RegAddr, Data);
}

static uint8_t MPU6050_ReadReg(uint8_t RegAddr)
{
    return SoftI2C_ReadReg(MPU6050_ADDRESS, RegAddr);
}

void MPU6050_Init(void)
{
    SoftI2C_Init();

    MPU6050_WriteReg(MPU6050_PWR_MGMT_1,   0x01);
    MPU6050_WriteReg(MPU6050_PWR_MGMT_2,   0x00);
    MPU6050_WriteReg(MPU6050_SMPLRT_DIV,   0x09);
    MPU6050_WriteReg(MPU6050_CONFIG,       0x06);
    MPU6050_WriteReg(MPU6050_GYRO_CONFIG,  0x18);   // ±2000 dps
    MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x10);   // ±8g
}

uint8_t MPU6050_GetID(void)
{
    return MPU6050_ReadReg(MPU6050_WHO_AM_I);
}

void MPU6050_GetData(int16_t *AccelX, int16_t *AccelY, int16_t *AccelZ,
                     int16_t *GyroX,  int16_t *GyroY,  int16_t *GyroZ)
{
    uint8_t Data[14];

    SoftI2C_ReadRegs(MPU6050_ADDRESS, MPU6050_ACCEL_XOUT_H, Data, 14);

    *AccelX = (int16_t)((Data[0]  << 8) | Data[1]);
    *AccelY = (int16_t)((Data[2]  << 8) | Data[3]);
    *AccelZ = (int16_t)((Data[4]  << 8) | Data[5]);

    *GyroX  = (int16_t)((Data[8]  << 8) | Data[9]);
    *GyroY  = (int16_t)((Data[10] << 8) | Data[11]);
    *GyroZ  = (int16_t)((Data[12] << 8) | Data[13]);
}