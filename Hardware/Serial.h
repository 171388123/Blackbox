#ifndef __SERIAL_H
#define __SERIAL_H

#include "stm32f10x.h"

void Serial_Init(void);
void Serial_SendByte(uint8_t Byte);
void Serial_SendString(char *String);
void Serial_SendNumber(uint32_t Number);

#endif