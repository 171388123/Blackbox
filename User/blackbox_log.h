#ifndef __BLACKBOXLOG_H
#define __BLACKBOXLOG_H

#include "stm32f10x.h"

typedef struct
{
    uint16_t Year;
    uint8_t Month;
    uint8_t Day;
    uint8_t Hour;
    uint8_t Minute;
    uint8_t Second;
} BlackBoxTime_t;

typedef struct
{
    uint32_t Magic;
    uint32_t Seq;
    uint32_t TickMs;
    uint32_t Event;
    int32_t  Pitch_x10;
    int32_t  Roll_x10;
    uint32_t AD;
    uint32_t GD;
    BlackBoxTime_t Time;
} BlackBoxRecord_t;



void BlackBoxLog_Init(void);
void BlackBoxLog_Format(void);
uint32_t BlackBoxLog_GetCount(void);
uint8_t BlackBoxLog_Append(BlackBoxTime_t *Time,
                           uint32_t TickMs,
                           uint32_t Event,
                           int32_t Pitch_x10,
                           int32_t Roll_x10,
                           uint32_t AD,
                           uint32_t GD);
uint8_t BlackBoxLog_ReadByIndex(uint32_t Index, BlackBoxRecord_t *Record);
uint8_t BlackBoxLog_ReadLast(BlackBoxRecord_t *Record);

#endif
