#include "alarm.h"
#include "Buzzer.h"
#include "motion_detect.h"

void Alarm_Init(void)
{
    Buzzer_Init();
    Buzzer_Off();
}

void Alarm_Update(uint32_t Event, uint32_t RunTimeMs)
{
    if (Event == MOTION_EVENT_NONE)
    {
        Buzzer_Off();
    }
    else if (Event == MOTION_EVENT_TILT)
    {
        if (((RunTimeMs / 500) % 2) == 0)
        {
            Buzzer_On();
        }
        else
        {
            Buzzer_Off();
        }
    }
    else if (Event == MOTION_EVENT_MOVE)
    {
        if (((RunTimeMs / 200) % 2) == 0)
        {
            Buzzer_On();
        }
        else
        {
            Buzzer_Off();
        }
    }
    else if (Event == MOTION_EVENT_SHOCK)
    {
        Buzzer_On();
    }
    else
    {
        Buzzer_Off();
    }
}