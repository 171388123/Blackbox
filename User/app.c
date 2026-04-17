#include "app.h"
#include "Delay.h"
#include "MPU6050.h"
#include "motion_detect.h"
#include "blackbox_log.h"
#include "ui.h"
#include "alarm.h"
#include "W25QXX.h"

#define APP_LOOP_PERIOD_MS    50

static MotionState_t App_State;
static uint32_t App_RunTimeMs = 0;
static uint32_t App_LastEvent = MOTION_EVENT_NONE;

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
    BlackBoxLog_Init();

    /* 第一次想清空日志时，临时取消注释，跑一次后再注释回去 */
    /* BlackBoxLog_Format(); */

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
        BlackBoxLog_Append(App_RunTimeMs,
                           App_State.Event,
                           App_State.Pitch_x10,
                           App_State.Roll_x10,
                           App_State.AccelDelta,
                           App_State.GyroAbsSum);
    }

    App_LastEvent = App_State.Event;

    UI_Update(&App_State, BlackBoxLog_GetCount());
    Alarm_Update(App_State.Event, App_RunTimeMs);

    App_RunTimeMs += APP_LOOP_PERIOD_MS;
}