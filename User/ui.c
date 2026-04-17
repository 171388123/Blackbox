#include "ui.h"
#include "OLED.h"

static void UI_ShowEventName(uint32_t Event)
{
    OLED_ShowString(4, 1, "EV:");

    if (Event == MOTION_EVENT_NONE)
    {
        OLED_ShowString(4, 4, "NORM ");
    }
    else if (Event == MOTION_EVENT_TILT)
    {
        OLED_ShowString(4, 4, "TILT ");
    }
    else if (Event == MOTION_EVENT_MOVE)
    {
        OLED_ShowString(4, 4, "MOVE ");
    }
    else if (Event == MOTION_EVENT_SHOCK)
    {
        OLED_ShowString(4, 4, "SHOCK");
    }
    else
    {
        OLED_ShowString(4, 4, "UNKN ");
    }
}

void UI_Init(void)
{
    OLED_Init();
    OLED_Clear();
}

void UI_ShowMessage(char *Line1, char *Line2, char *Line3, char *Line4)
{
    OLED_Clear();

    if (Line1[0] != '\0') OLED_ShowString(1, 1, Line1);
    if (Line2[0] != '\0') OLED_ShowString(2, 1, Line2);
    if (Line3[0] != '\0') OLED_ShowString(3, 1, Line3);
    if (Line4[0] != '\0') OLED_ShowString(4, 1, Line4);
}

void UI_Update(MotionState_t *State, uint32_t LogCount)
{
    OLED_ShowString(1, 1, "P:");
    OLED_ShowSignedNum(1, 3, State->Pitch_x10 / 10, 4);
    OLED_ShowString(1, 8, "R:");
    OLED_ShowSignedNum(1, 10, State->Roll_x10 / 10, 4);

    OLED_ShowString(2, 1, "AD:");
    OLED_ShowNum(2, 4, State->AccelDelta, 5);

    OLED_ShowString(3, 1, "GD:");
    OLED_ShowNum(3, 4, State->GyroAbsSum, 5);

    UI_ShowEventName(State->Event);

    OLED_ShowString(4, 11, "L:");
    OLED_ShowNum(4, 13, LogCount, 3);
}