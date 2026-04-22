#ifndef __SERIAL_H
#define __SERIAL_H

#include "stm32f10x.h"

void Serial_Init(void);
void Serial_SendByte(uint8_t Byte);
void Serial_SendString(const char *String);
void Serial_SendNumber(uint32_t Number);

uint8_t Serial_GetRxFlag(void);
uint8_t Serial_GetRxData(void);
uint16_t Serial_GetRxOverflowCount(void);

#endif