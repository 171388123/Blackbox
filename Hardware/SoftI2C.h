#ifndef __SOFTI2C_H
#define __SOFTI2C_H

#include "stm32f10x.h"

void SoftI2C_Init(void);
void SoftI2C_Start(void);
void SoftI2C_Stop(void);
uint8_t SoftI2C_SendByte(uint8_t Byte);
uint8_t SoftI2C_ReceiveByte(void);
void SoftI2C_SendAck(uint8_t AckBit);

uint8_t SoftI2C_WriteReg(uint8_t DevAddr, uint8_t RegAddr, uint8_t Data);
uint8_t SoftI2C_ReadReg(uint8_t DevAddr, uint8_t RegAddr);
void SoftI2C_ReadRegs(uint8_t DevAddr, uint8_t RegAddr, uint8_t *DataArray, uint8_t Length);

#endif