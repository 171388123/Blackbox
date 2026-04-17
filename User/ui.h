#ifndef __UI_H
#define __UI_H

#include "stm32f10x.h"
#include "motion_detect.h"

void UI_Init(void);
void UI_ShowMessage(char *Line1, char *Line2, char *Line3, char *Line4);
void UI_Update(MotionState_t *State, uint32_t LogCount);

#endif