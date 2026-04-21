#include "app.h"
#include "Delay.h"
#include "MPU6050.h"
#include "motion_detect.h"
#include "blackbox_log.h"
#include "ui.h"
#include "alarm.h"
#include "W25QXX.h"
#include "Serial.h"

#define APP_LOOP_PERIOD_MS 50

/* 如果你的 Serial.h 里还没声明这两个函数，
   这两个 extern 可以先顶着用，避免 app.c 报未声明 */
extern uint8_t Serial_GetRxFlag(void);
extern uint8_t Serial_GetRxData(void);

static MotionState_t App_State;
static uint32_t App_RunTimeMs = 0;
static uint8_t App_LastEvent = MOTION_EVENT_NONE;

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

static void App_SendSignedNumber(int32_t Number)
{
    if (Number < 0)
    {
        Serial_SendByte('-');
        Number = -Number;
    }
    Serial_SendNumber((uint32_t)Number);
}

static void App_SendX10(int16_t ValueX10)
{
    int16_t IntPart;
    int16_t DecPart;

    if (ValueX10 < 0)
    {
        Serial_SendByte('-');
        ValueX10 = -ValueX10;
    }

    IntPart = ValueX10 / 10;
    DecPart = ValueX10 % 10;

    Serial_SendNumber((uint32_t)IntPart);
    Serial_SendByte('.');
    Serial_SendNumber((uint32_t)DecPart);
}

static void App_PrintHelp(void)
{
    Serial_SendString("[CMD] H/?=help, N=now, L=last, C=clear\r\n");
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

static void App_PrintNowState(void)
{
    Serial_SendString("[NOW] Tick=");
    Serial_SendNumber(App_RunTimeMs);
    Serial_SendString(" ms, EV=");
    Serial_SendString(App_EventToString(App_State.Event));

    Serial_SendString(", Pitch=");
    App_SendX10(App_State.Pitch_x10);

    Serial_SendString(", Roll=");
    App_SendX10(App_State.Roll_x10);

    Serial_SendString(", AD=");
    Serial_SendNumber(App_State.AccelDelta);

    Serial_SendString(", GD=");
    Serial_SendNumber(App_State.GyroAbsSum);

    Serial_SendString(", LogCount=");
    Serial_SendNumber(BlackBoxLog_GetCount());
    Serial_SendString("\r\n");
}

static void App_PrintLastLog(void)
{
    BlackBoxRecord_t Record;

    if (BlackBoxLog_GetCount() == 0)
    {
        Serial_SendString("[CMD] No log\r\n");
        return;
    }

    if (BlackBoxLog_ReadLast(&Record))
    {
        Serial_SendString("[LAST] Seq=");
        Serial_SendNumber(Record.Seq);
        Serial_SendString(", Tick=");
        Serial_SendNumber(Record.TickMs);
        Serial_SendString(" ms, EV=");
        Serial_SendString(App_EventToString(Record.Event));

        Serial_SendString(", Pitch=");
        App_SendX10(Record.Pitch_x10);

        Serial_SendString(", Roll=");
        App_SendX10(Record.Roll_x10);

        Serial_SendString(", AD=");
        Serial_SendNumber(Record.AD);

        Serial_SendString(", GD=");
        Serial_SendNumber(Record.GD);

        Serial_SendString("\r\n");
    }
    else
    {
        Serial_SendString("[CMD] Read last log failed\r\n");
    }
}

static void App_HandleSerialCmd(void)
{
    uint8_t Cmd;

    if (Serial_GetRxFlag() == 0)
    {
        return;
    }

    Cmd = Serial_GetRxData();

    /* 过滤串口工具顺手发过来的回车换行 */
    if ((Cmd == '\r') || (Cmd == '\n') || (Cmd == ' '))
    {
        return;
    }

    /* 回显一下，方便你调试“到底收没收到” */
    Serial_SendString("[RX] ");
    Serial_SendByte(Cmd);
    Serial_SendString("\r\n");

    if ((Cmd >= 'a') && (Cmd <= 'z'))
    {
        Cmd = Cmd - 'a' + 'A';
    }

    switch (Cmd)
    {
    case 'H':
    case '?':
        App_PrintHelp();
        break;

    case 'N':
        App_PrintNowState();
        break;

    case 'L':
        App_PrintLastLog();
        break;

    case 'C':
        BlackBoxLog_Format();

        /* 很关键：
           清空日志时，把“上一状态”同步成当前状态，
           防止刚清完又因为状态切换判断立刻补写一条 */
        App_LastEvent = App_State.Event;

        Serial_SendString("[CMD] Logs cleared, count=0\r\n");
        break;

    default:
        Serial_SendString("[CMD] Unknown cmd: ");
        Serial_SendByte(Cmd);
        Serial_SendString("\r\n");
        App_PrintHelp();
        break;
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

    App_RunTimeMs = 0;
    App_LastEvent = MOTION_EVENT_NONE;

    App_State.Pitch_x10 = 0;
    App_State.Roll_x10 = 0;
    App_State.AccelDelta = 0;
    App_State.GyroAbsSum = 0;
    App_State.Event = MOTION_EVENT_NONE;

    /* 串口放最前面，后面谁炸了都能看到 */
    Serial_Init();
    Serial_SendString("[BOOT] Serial OK\r\n");

    UI_Init();
    Serial_SendString("[INIT] UI OK\r\n");

    Alarm_Init();
    Serial_SendString("[INIT] Alarm OK\r\n");

    MPU6050_Init();
    Serial_SendString("[INIT] MPU6050 OK\r\n");

    W25Qxx_Init();
    Serial_SendString("[INIT] W25QXX OK\r\n");

    BlackBoxLog_Init();
    Serial_SendString("[INIT] BlackBox OK\r\n");

    App_PrintLastLogOnBoot();

    UI_ShowMessage("BlackBox Init", "Calibrating...", "Keep Still", "");
    Serial_SendString("[INIT] Motion Calibrating...\r\n");

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

    Serial_SendString("[INIT] Motion Baseline OK\r\n");

    UI_ShowMessage("Init OK", "Monitoring...", "", "");
    Serial_SendString("[BOOT] System Init OK\r\n");
    App_PrintHelp();

    Delay_ms(500);
}

void App_Loop(void)
{
    int16_t Ax, Ay, Az;
    int16_t Gx, Gy, Gz;

    MPU6050_GetData(&Ax, &Ay, &Az, &Gx, &Gy, &Gz);

    MotionDetect_Update(&App_State, Ax, Ay, Az, Gx, Gy, Gz);

    if (App_State.Event != App_LastEvent)
    {
        Serial_SendString("[EV] ");
        Serial_SendNumber(App_RunTimeMs);
        Serial_SendString(" ms, ");
        Serial_SendString(App_EventToString(App_LastEvent));
        Serial_SendString(" -> ");
        Serial_SendString(App_EventToString(App_State.Event));
        Serial_SendString("\r\n");

        if (App_State.Event != MOTION_EVENT_NONE)
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

        App_LastEvent = App_State.Event;
    }

    App_HandleSerialCmd();

    UI_Update(&App_State, BlackBoxLog_GetCount());
    Alarm_Update(App_State.Event, App_RunTimeMs);

    Delay_ms(APP_LOOP_PERIOD_MS);
    App_RunTimeMs += APP_LOOP_PERIOD_MS;
}