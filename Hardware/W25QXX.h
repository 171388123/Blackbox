#ifndef __W25QXX_H
#define __W25QXX_H

#include "stm32f10x.h"

void W25Qxx_Init(void);
uint32_t W25Qxx_ReadID(void);
void W25Qxx_ReadData(uint32_t Address, uint8_t *Buffer, uint16_t Count);
void W25Qxx_PageProgram(uint32_t Address, uint8_t *Buffer, uint16_t Count);
void W25Qxx_SectorErase(uint32_t Address);

#endif