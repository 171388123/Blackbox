#include "app.h"
#include "Delay.h"
#include "MPU6050.h"
#include "motion_detect.h"
#include "blackbox_log.h"
#include "ui.h"
#include "alarm.h"
#include "W25QXX.h"
#include "serial.h"

#define APP_LOOP_PERIOD_MS 50

static MotionState_t App_State;
static uint32_t App_RunTimeMs = 0;
static uint32_t App_LastEvent = MOTION_EVENT_NONE;

static char *App_EventToString(uint8_t Event)
{
    switch (Event)
    {
    case MOTION_EVENT_NONE:
        return "NORM";
    case MOTION_EVENT_TILT:
        return "TILT";
    case MOTION_EVENT_MOVE:
        return "MOVE";
    case MOTION_EVENT_SHOCK:
        return "SHOCK";
    default:
        return "UNK";
    }
}

static void App_PrintLastLogOnBoot(void)
{
    BlackBoxRecord_t LastRecord;

    Serial_SendString("[BOOT] LogCount=");
    Serial_SendNumber(BlackBoxLog_GetCount());
    Serial_SendString("\r\n");

    if (BlackBoxLog_ReadLast(&LastRecord))
    {
        Serial_SendString("[LAST] Seq=");
        Serial_SendNumber(LastRecord.Seq);
        Serial_SendString(", Tick=");
        Serial_SendNumber(LastRecord.TickMs);
        Serial_SendString(" ms, EV=");
        Serial_SendString(App_EventToString(LastRecord.Event));
        Serial_SendString("\r\n");
    }
    else
    {
        Serial_SendString("[LAST] No Log\r\n");
    }
}

void App_Init(void)
{
    int16_t Ax, Ay, Az;
    int16_t Gx, Gy, Gz;
    int32_t SumAx = 0;
    int32_t SumAy = 0;
    int32_t SumAz = 0;
    uint16_t i;

    UI_Init();
    Alarm_Init();
    MPU6050_Init();
    W25Qxx_Init();

    Serial_Init();
    Serial_SendString("BlackBox Boot OK\r\n");

    BlackBoxLog_Init();
    App_PrintLastLogOnBoot();

    UI_ShowMessage("BlackBox Init", "Calibrating...", "Keep Still", "");

    for (i = 0; i < 100; i++)
    {
        MPU6050_GetData(&Ax, &Ay, &Az, &Gx, &Gy, &Gz);
        SumAx += Ax;
        SumAy += Ay;
        SumAz += Az;
        Delay_ms(5);
    }

    MotionDetect_SetBaseline((int16_t)(SumAx / 100),
                             (int16_t)(SumAy / 100),
                             (int16_t)(SumAz / 100));

    App_RunTimeMs = 0;
    App_LastEvent = MOTION_EVENT_NONE;

    UI_ShowMessage("Init OK", "Monitoring...", "", "");
    Serial_SendString("System Init OK\r\n");
    Delay_ms(500);
}

void App_Loop(void)
{
    int16_t Ax, Ay, Az;
    int16_t Gx, Gy, Gz;

    MPU6050_GetData(&Ax, &Ay, &Az, &Gx, &Gy, &Gz);

    MotionDetect_Update(&App_State, Ax, Ay, Az, Gx, Gy, Gz);

    /* 只在“进入异常”或“异常类型变化”时记一条日志 */
    if ((App_State.Event != MOTION_EVENT_NONE) && (App_State.Event != App_LastEvent))
    {
        if (BlackBoxLog_Append(App_RunTimeMs,
                               App_State.Event,
                               App_State.Pitch_x10,
                               App_State.Roll_x10,
                               App_State.AccelDelta,
                               App_State.GyroAbsSum))
        {
            Serial_SendString("[LOG] #");
            Serial_SendNumber(BlackBoxLog_GetCount());
            Serial_SendString(", ");
            Serial_SendNumber(App_RunTimeMs);
            Serial_SendString(" ms, EV=");
            Serial_SendString(App_EventToString(App_State.Event));
            Serial_SendString("\r\n");
        }
        else
        {
            Serial_SendString("[LOG] Append Failed\r\n");
        }
    }
    if (App_State.Event != App_LastEvent)
    {
        Serial_SendString("[EV] ");
        Serial_SendNumber(App_RunTimeMs);
        Serial_SendString(" ms, ");
        Serial_SendString(App_EventToString(App_LastEvent));
        Serial_SendString(" -> ");
        Serial_SendString(App_EventToString(App_State.Event));
        Serial_SendString("\r\n");
    }

    App_LastEvent = App_State.Event;

    UI_Update(&App_State, BlackBoxLog_GetCount());
    Alarm_Update(App_State.Event, App_RunTimeMs);

    App_RunTimeMs += APP_LOOP_PERIOD_MS;
}