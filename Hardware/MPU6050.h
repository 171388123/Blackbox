#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f10x.h"

void MPU6050_Init(void);
uint8_t MPU6050_GetID(void);
void MPU6050_GetData(int16_t *AccelX, int16_t *AccelY, int16_t *AccelZ,
                     int16_t *GyroX,  int16_t *GyroY,  int16_t *GyroZ);

#endif