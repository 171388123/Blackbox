#ifndef __ALARM_H
#define __ALARM_H

#include "stm32f10x.h"

void Alarm_Init(void);
void Alarm_Update(uint32_t Event, uint32_t RunTimeMs);

#endif