#ifndef __MOTION_DETECT_H
#define __MOTION_DETECT_H

#include "stm32f10x.h"

#define MOTION_EVENT_NONE     0
#define MOTION_EVENT_TILT     1
#define MOTION_EVENT_MOVE     2
#define MOTION_EVENT_SHOCK    3

typedef struct
{
    int16_t Pitch_x10;
    int16_t Roll_x10;
    uint16_t AccelDelta;
    uint16_t GyroAbsSum;
    uint8_t Event;
} MotionState_t;

void MotionDetect_SetBaseline(int16_t Ax, int16_t Ay, int16_t Az);
void MotionDetect_Update(MotionState_t *State,
                         int16_t Ax, int16_t Ay, int16_t Az,
                         int16_t Gx, int16_t Gy, int16_t Gz);

#endif