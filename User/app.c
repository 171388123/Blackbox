#include "app.h"
#include "Delay.h"
#include "MPU6050.h"
#include "motion_detect.h"
#include "blackbox_log.h"
#include "ui.h"
#include "alarm.h"
#include "W25QXX.h"
#include "Serial.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "misc.h"

#define APP_LOOP_PERIOD_MS 50
#define APP_CMD_BUF_SIZE 64

typedef BlackBoxTime_t AppTime_t;
static AppTime_t App_CurrentTime = {2026, 4, 22, 16, 0, 0};
static uint16_t App_TimeMsAcc = 0;

static MotionState_t App_State;
static uint32_t App_RunTimeMs = 0;
static uint8_t App_LastEvent = MOTION_EVENT_NONE;
static volatile uint32_t App_RealMsTick = 0;
static uint32_t App_LastRealMsTick = 0;
static char App_CmdBuf[APP_CMD_BUF_SIZE];
static uint8_t App_CmdIndex = 0;

static void App_RealTimeBaseInit(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseStructure.TIM_Prescaler = 7200 - 1; /* 72MHz / 7200 = 10kHz */
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = 10 - 1; /* 10kHz / 10 = 1kHz -> 1ms */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM2, ENABLE);
}

static void App_SyncTimeByRealTick(void)
{
    uint32_t NowMs;
    uint32_t DeltaMs;

    NowMs = App_RealMsTick;
    DeltaMs = NowMs - App_LastRealMsTick;
    App_LastRealMsTick = NowMs;

    App_RunTimeMs = NowMs;

    if (DeltaMs > 0)
    {
        App_TimeMsAcc += DeltaMs;

        while (App_TimeMsAcc >= 1000)
        {
            App_TimeMsAcc -= 1000;
            App_CurrentTime.Second++;

            if (App_CurrentTime.Second >= 60)
            {
                App_CurrentTime.Second = 0;
                App_CurrentTime.Minute++;

                if (App_CurrentTime.Minute >= 60)
                {
                    App_CurrentTime.Minute = 0;
                    App_CurrentTime.Hour++;

                    if (App_CurrentTime.Hour >= 24)
                    {
                        App_CurrentTime.Hour = 0;
                        App_CurrentTime.Day++;

                        /* 这里先简化处理，不跨月底就够你当前实验用了 */
                    }
                }
            }
        }
    }
}

void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        App_RealMsTick++;
    }
}

static uint8_t App_IsLeapYear(uint16_t Year)
{
    if ((Year % 400) == 0)
        return 1;
    if ((Year % 100) == 0)
        return 0;
    if ((Year % 4) == 0)
        return 1;
    return 0;
}

static uint8_t App_GetMonthDays(uint16_t Year, uint8_t Month)
{
    static const uint8_t Days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (Month == 2)
    {
        return App_IsLeapYear(Year) ? 29 : 28;
    }

    if ((Month >= 1) && (Month <= 12))
    {
        return Days[Month - 1];
    }

    return 30;
}

static void App_TimeTick1s(AppTime_t *Time)
{
    Time->Second++;

    if (Time->Second >= 60)
    {
        Time->Second = 0;
        Time->Minute++;

        if (Time->Minute >= 60)
        {
            Time->Minute = 0;
            Time->Hour++;

            if (Time->Hour >= 24)
            {
                Time->Hour = 0;
                Time->Day++;

                if (Time->Day > App_GetMonthDays(Time->Year, Time->Month))
                {
                    Time->Day = 1;
                    Time->Month++;

                    if (Time->Month > 12)
                    {
                        Time->Month = 1;
                        Time->Year++;
                    }
                }
            }
        }
    }
}

static void App_Send2Digits(uint8_t Value)
{
    Serial_SendByte((Value / 10) + '0');
    Serial_SendByte((Value % 10) + '0');
}

static void App_PrintTime(const AppTime_t *Time)
{
    Serial_SendNumber(Time->Year);
    Serial_SendByte('-');
    App_Send2Digits(Time->Month);
    Serial_SendByte('-');
    App_Send2Digits(Time->Day);
    Serial_SendByte(' ');
    App_Send2Digits(Time->Hour);
    Serial_SendByte(':');
    App_Send2Digits(Time->Minute);
    Serial_SendByte(':');
    App_Send2Digits(Time->Second);
}

static const char *App_EventToString(uint8_t Event)
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

static void App_SendProtoTime(const AppTime_t *Time)
{
    Serial_SendNumber(Time->Year);
    Serial_SendByte('-');
    App_Send2Digits(Time->Month);
    Serial_SendByte('-');
    App_Send2Digits(Time->Day);
    Serial_SendByte('_');
    App_Send2Digits(Time->Hour);
    Serial_SendByte(':');
    App_Send2Digits(Time->Minute);
    Serial_SendByte(':');
    App_Send2Digits(Time->Second);
}

static char App_ToUpperChar(char Ch)
{
    if ((Ch >= 'a') && (Ch <= 'z'))
    {
        return (char)(Ch - 'a' + 'A');
    }
    return Ch;
}

static uint8_t App_StrEqualIgnoreCase(const char *A, const char *B)
{
    while ((*A != '\0') && (*B != '\0'))
    {
        if (App_ToUpperChar(*A) != App_ToUpperChar(*B))
        {
            return 0;
        }
        A++;
        B++;
    }

    return ((*A == '\0') && (*B == '\0')) ? 1 : 0;
}

static uint8_t App_StrStartsWithIgnoreCase(const char *Str, const char *Prefix)
{
    while (*Prefix != '\0')
    {
        if (*Str == '\0')
        {
            return 0;
        }

        if (App_ToUpperChar(*Str) != App_ToUpperChar(*Prefix))
        {
            return 0;
        }

        Str++;
        Prefix++;
    }

    return 1;
}

static void App_SendProtoOk(const char *Text)
{
    Serial_SendString("OK,");
    Serial_SendString(Text);
    Serial_SendString("\r\n");
}

static void App_SendProtoErr(const char *Text)
{
    Serial_SendString("ERR,");
    Serial_SendString(Text);
    Serial_SendString("\r\n");
}

static void App_SendProtoStat(const char *Prefix)
{
    Serial_SendString(Prefix);

    Serial_SendString(",TIME=");
    App_SendProtoTime(&App_CurrentTime);

    Serial_SendString(",TICK=");
    Serial_SendNumber(App_RunTimeMs);

    Serial_SendString(",EV=");
    Serial_SendString(App_EventToString(App_State.Event));

    Serial_SendString(",PITCH=");
    App_SendX10(App_State.Pitch_x10);

    Serial_SendString(",ROLL=");
    App_SendX10(App_State.Roll_x10);

    Serial_SendString(",AD=");
    Serial_SendNumber(App_State.AccelDelta);

    Serial_SendString(",GD=");
    Serial_SendNumber(App_State.GyroAbsSum);

    Serial_SendString(",LOG=");
    Serial_SendNumber(BlackBoxLog_GetCount());

    Serial_SendString("\r\n");
}

static void App_SendProtoLast(void)
{
    BlackBoxRecord_t Record;

    if (!BlackBoxLog_ReadLast(&Record))
    {
        Serial_SendString("LAST,NONE\r\n");
        return;
    }

    Serial_SendString("LAST,SEQ=");
    Serial_SendNumber(Record.Seq);

    Serial_SendString(",TIME=");
    App_SendProtoTime(&Record.Time);

    Serial_SendString(",TICK=");
    Serial_SendNumber(Record.TickMs);

    Serial_SendString(",EV=");
    Serial_SendString(App_EventToString((uint8_t)Record.Event));

    Serial_SendString(",PITCH=");
    App_SendX10((int16_t)Record.Pitch_x10);

    Serial_SendString(",ROLL=");
    App_SendX10((int16_t)Record.Roll_x10);

    Serial_SendString(",AD=");
    Serial_SendNumber(Record.AD);

    Serial_SendString(",GD=");
    Serial_SendNumber(Record.GD);

    Serial_SendString("\r\n");
}

static uint8_t App_IsLegacySingleCmd(uint8_t Ch)
{
    char Cmd;

    Cmd = App_ToUpperChar((char)Ch);

    return (Cmd == 'H') || (Cmd == '?') ||
           (Cmd == 'N') || (Cmd == 'L') ||
           (Cmd == 'C');
}

static void App_PrintHelp(void)
{
    Serial_SendString("[CMD] HELP / PING / GET / LAST / CLR\r\n");
    Serial_SendString("[CMD] T yyyy-mm-dd hh:mm:ss\r\n");
    Serial_SendString("[OLD] H/?/N/L/C still supported\r\n");
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
        Serial_SendString(App_EventToString((uint8_t)LastRecord.Event));
        Serial_SendString("\r\n");
    }
    else
    {
        Serial_SendString("[LAST] No Log\r\n");
    }
}

static void App_PrintNowState(void)
{
    Serial_SendString("[NOW] Time=");
    App_PrintTime(&App_CurrentTime);

    Serial_SendString(", Tick=");
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
        Serial_SendString(App_EventToString((uint8_t)Record.Event));

        Serial_SendString(", Pitch=");
        App_SendX10((int16_t)Record.Pitch_x10);

        Serial_SendString(", Roll=");
        App_SendX10((int16_t)Record.Roll_x10);

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

static uint8_t App_IsDigit(char Ch)
{
    return (Ch >= '0') && (Ch <= '9');
}

static uint16_t App_Parse4Digits(const char *Str)
{
    return (uint16_t)((Str[0] - '0') * 1000 +
                      (Str[1] - '0') * 100 +
                      (Str[2] - '0') * 10 +
                      (Str[3] - '0'));
}

static uint8_t App_Parse2Digits(const char *Str)
{
    return (uint8_t)((Str[0] - '0') * 10 +
                     (Str[1] - '0'));
}

static uint8_t App_ParseTimeString(const char *Str, AppTime_t *Time)
{
    uint8_t i;

    if (Str == 0 || Time == 0)
    {
        return 0;
    }

    while (*Str == ' ')
    {
        Str++;
    }

    for (i = 0; i < 19; i++)
    {
        if (Str[i] == '\0')
        {
            return 0;
        }
    }

    if (Str[19] != '\0')
    {
        return 0;
    }

    if ((!App_IsDigit(Str[0])) || (!App_IsDigit(Str[1])) ||
        (!App_IsDigit(Str[2])) || (!App_IsDigit(Str[3])) ||
        (Str[4] != '-') ||
        (!App_IsDigit(Str[5])) || (!App_IsDigit(Str[6])) ||
        (Str[7] != '-') ||
        (!App_IsDigit(Str[8])) || (!App_IsDigit(Str[9])) ||
        (Str[10] != ' ') ||
        (!App_IsDigit(Str[11])) || (!App_IsDigit(Str[12])) ||
        (Str[13] != ':') ||
        (!App_IsDigit(Str[14])) || (!App_IsDigit(Str[15])) ||
        (Str[16] != ':') ||
        (!App_IsDigit(Str[17])) || (!App_IsDigit(Str[18])))
    {
        return 0;
    }

    Time->Year = App_Parse4Digits(&Str[0]);
    Time->Month = App_Parse2Digits(&Str[5]);
    Time->Day = App_Parse2Digits(&Str[8]);
    Time->Hour = App_Parse2Digits(&Str[11]);
    Time->Minute = App_Parse2Digits(&Str[14]);
    Time->Second = App_Parse2Digits(&Str[17]);

    if ((Time->Month < 1) || (Time->Month > 12))
    {
        return 0;
    }

    if ((Time->Day < 1) || (Time->Day > App_GetMonthDays(Time->Year, Time->Month)))
    {
        return 0;
    }

    if (Time->Hour > 23)
    {
        return 0;
    }

    if (Time->Minute > 59)
    {
        return 0;
    }

    if (Time->Second > 59)
    {
        return 0;
    }

    return 1;
}

static void App_ResetCmdBuffer(void)
{
    App_CmdIndex = 0;
    App_CmdBuf[0] = '\0';
}

static void App_SetCurrentTime(const AppTime_t *Time)
{
    App_CurrentTime = *Time;
    App_TimeMsAcc = 0;
}

static void App_HandleCmdLine(char *Line)
{
    AppTime_t NewTime;
    char *Cmd;
    char *End;

    Cmd = Line;
    while (*Cmd == ' ')
    {
        Cmd++;
    }

    End = Cmd;
    while (*End != '\0')
    {
        End++;
    }

    while ((End > Cmd) && (*(End - 1) == ' '))
    {
        End--;
    }
    *End = '\0';

    if (*Cmd == '\0')
    {
        return;
    }

    if (App_StrEqualIgnoreCase(Cmd, "PING"))
    {
        App_SendProtoOk("PONG");
        return;
    }

    if (App_StrEqualIgnoreCase(Cmd, "GET") || App_StrEqualIgnoreCase(Cmd, "N"))
    {
        App_SendProtoStat("STAT");
        return;
    }

    if (App_StrEqualIgnoreCase(Cmd, "LAST") || App_StrEqualIgnoreCase(Cmd, "L"))
    {
        App_SendProtoLast();
        return;
    }

    if (App_StrEqualIgnoreCase(Cmd, "CLR") || App_StrEqualIgnoreCase(Cmd, "C"))
    {
        BlackBoxLog_Format();
        App_LastEvent = App_State.Event;
        App_SendProtoOk("CLR");
        return;
    }

    if (App_StrEqualIgnoreCase(Cmd, "HELP") ||
        App_StrEqualIgnoreCase(Cmd, "H") ||
        App_StrEqualIgnoreCase(Cmd, "?"))
    {
        App_PrintHelp();
        return;
    }

    if (App_StrStartsWithIgnoreCase(Cmd, "T "))
    {
        if (App_ParseTimeString(Cmd + 2, &NewTime))
        {
            App_SetCurrentTime(&NewTime);
            Serial_SendString("OK,TIME=");
            App_SendProtoTime(&App_CurrentTime);
            Serial_SendString("\r\n");
        }
        else
        {
            App_SendProtoErr("BAD_TIME");
        }
        return;
    }

    App_SendProtoErr("UNKNOWN");
}

static void App_HandleSerialCmd(void)
{
    uint8_t Ch;

    while (Serial_GetRxFlag())
    {
        Ch = Serial_GetRxData();

        if ((Ch == '\r') || (Ch == '\n'))
        {
            if (App_CmdIndex > 0)
            {
                App_CmdBuf[App_CmdIndex] = '\0';
                App_HandleCmdLine(App_CmdBuf);
                App_ResetCmdBuffer();
            }
            continue;
        }

        if ((App_CmdIndex == 0) && App_IsLegacySingleCmd(Ch) && (!Serial_GetRxFlag()))
        {
            App_CmdBuf[0] = (char)Ch;
            App_CmdBuf[1] = '\0';
            App_HandleCmdLine(App_CmdBuf);
            App_ResetCmdBuffer();
            continue;
        }

        if (App_CmdIndex < (APP_CMD_BUF_SIZE - 1))
        {
            App_CmdBuf[App_CmdIndex++] = (char)Ch;
            App_CmdBuf[App_CmdIndex] = '\0';
        }
        else
        {
            App_SendProtoErr("TOO_LONG");
            App_ResetCmdBuffer();
        }
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
    App_TimeMsAcc = 0;
    App_LastEvent = MOTION_EVENT_NONE;
    App_ResetCmdBuffer();

    App_State.Pitch_x10 = 0;
    App_State.Roll_x10 = 0;
    App_State.AccelDelta = 0;
    App_State.GyroAbsSum = 0;
    App_State.Event = MOTION_EVENT_NONE;

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

    App_RealTimeBaseInit();
    App_LastRealMsTick = App_RealMsTick;

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
    App_SyncTimeByRealTick();
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
        App_SendProtoStat("EVT");
        if (App_State.Event != MOTION_EVENT_NONE)
        {
            if (BlackBoxLog_Append(&App_CurrentTime,
                                   App_RunTimeMs,
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
}
