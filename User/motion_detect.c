#include "motion_detect.h"
#include <math.h>

#define RAD_TO_DEG_X10            572.9578f

/* 阈值 */
#define TILT_THRESHOLD_X10        180      /* 18.0° */
#define MOVE_ACCEL_THRESHOLD      1200
#define MOVE_GYRO_THRESHOLD       2000
#define SHOCK_ACCEL_THRESHOLD     5000
#define SHOCK_GYRO_THRESHOLD      7000

/* 连续命中 / 释放计数 */
#define MOVE_ASSERT_COUNT         2
#define TILT_ASSERT_COUNT         2
#define SHOCK_ASSERT_COUNT        1
#define EVENT_RELEASE_COUNT       2

static int16_t BaselineAx = 0;
static int16_t BaselineAy = 0;
static int16_t BaselineAz = 0;
static int16_t BaselinePitch_x10 = 0;
static int16_t BaselineRoll_x10 = 0;

/* 事件状态机内部变量 */
static uint8_t StableEvent = MOTION_EVENT_NONE;
static uint8_t CandidateEvent = MOTION_EVENT_NONE;
static uint8_t CandidateCount = 0;
static uint8_t ReleaseCount = 0;

static uint16_t MotionDetect_Abs16(int16_t Value)
{
    if (Value < 0)
    {
        return (uint16_t)(-Value);
    }
    return (uint16_t)Value;
}

static int16_t MotionDetect_CalcPitch_x10(int16_t Ax, int16_t Ay, int16_t Az)
{
    float Pitch;
    (void)Ax;

    Pitch = atan2f((float)Ay, (float)Az) * RAD_TO_DEG_X10;
    return (int16_t)Pitch;
}

static int16_t MotionDetect_CalcRoll_x10(int16_t Ax, int16_t Ay, int16_t Az)
{
    float Roll;

    Roll = atan2f(-(float)Ax, sqrtf((float)Ay * Ay + (float)Az * Az)) * RAD_TO_DEG_X10;
    return (int16_t)Roll;
}

static uint8_t MotionDetect_GetRawEvent(uint16_t PitchDelta,
                                        uint16_t RollDelta,
                                        uint16_t AccelDelta,
                                        uint16_t GyroAbsSum)
{
    if ((AccelDelta > SHOCK_ACCEL_THRESHOLD) || (GyroAbsSum > SHOCK_GYRO_THRESHOLD))
    {
        return MOTION_EVENT_SHOCK;
    }

    if ((PitchDelta > TILT_THRESHOLD_X10) || (RollDelta > TILT_THRESHOLD_X10))
    {
        return MOTION_EVENT_TILT;
    }

    if ((AccelDelta > MOVE_ACCEL_THRESHOLD) || (GyroAbsSum > MOVE_GYRO_THRESHOLD))
    {
        return MOTION_EVENT_MOVE;
    }

    return MOTION_EVENT_NONE;
}

static uint8_t MotionDetect_GetEventPriority(uint8_t Event)
{
    switch (Event)
    {
    case MOTION_EVENT_MOVE:
        return 1;

    case MOTION_EVENT_TILT:
        return 2;

    case MOTION_EVENT_SHOCK:
        return 3;

    default:
        return 0;
    }
}

static uint8_t MotionDetect_GetAssertCount(uint8_t Event)
{
    switch (Event)
    {
    case MOTION_EVENT_MOVE:
        return MOVE_ASSERT_COUNT;

    case MOTION_EVENT_TILT:
        return TILT_ASSERT_COUNT;

    case MOTION_EVENT_SHOCK:
        return SHOCK_ASSERT_COUNT;

    default:
        return 0;
    }
}

void MotionDetect_SetBaseline(int16_t Ax, int16_t Ay, int16_t Az)
{
    BaselineAx = Ax;
    BaselineAy = Ay;
    BaselineAz = Az;

    BaselinePitch_x10 = MotionDetect_CalcPitch_x10(Ax, Ay, Az);
    BaselineRoll_x10  = MotionDetect_CalcRoll_x10(Ax, Ay, Az);

    StableEvent = MOTION_EVENT_NONE;
    CandidateEvent = MOTION_EVENT_NONE;
    CandidateCount = 0;
    ReleaseCount = 0;
}

void MotionDetect_Update(MotionState_t *State,
                         int16_t Ax, int16_t Ay, int16_t Az,
                         int16_t Gx, int16_t Gy, int16_t Gz)
{
    uint16_t PitchDelta;
    uint16_t RollDelta;
    uint8_t RawEvent;
    uint8_t RawPriority;
    uint8_t StablePriority;
    uint8_t NeedCount;

    State->Pitch_x10 = MotionDetect_CalcPitch_x10(Ax, Ay, Az);
    State->Roll_x10  = MotionDetect_CalcRoll_x10(Ax, Ay, Az);

    State->AccelDelta =
        MotionDetect_Abs16(Ax - BaselineAx) +
        MotionDetect_Abs16(Ay - BaselineAy) +
        MotionDetect_Abs16(Az - BaselineAz);

    State->GyroAbsSum =
        MotionDetect_Abs16(Gx) +
        MotionDetect_Abs16(Gy) +
        MotionDetect_Abs16(Gz);

    PitchDelta = MotionDetect_Abs16(State->Pitch_x10 - BaselinePitch_x10);
    RollDelta  = MotionDetect_Abs16(State->Roll_x10  - BaselineRoll_x10);

    RawEvent = MotionDetect_GetRawEvent(PitchDelta,
                                        RollDelta,
                                        State->AccelDelta,
                                        State->GyroAbsSum);

    if (RawEvent == StableEvent)
    {
        CandidateEvent = MOTION_EVENT_NONE;
        CandidateCount = 0;
        ReleaseCount = 0;
    }
    else if (RawEvent == MOTION_EVENT_NONE)
    {
        CandidateEvent = MOTION_EVENT_NONE;
        CandidateCount = 0;

        if (StableEvent != MOTION_EVENT_NONE)
        {
            ReleaseCount++;

            if (ReleaseCount >= EVENT_RELEASE_COUNT)
            {
                StableEvent = MOTION_EVENT_NONE;
                ReleaseCount = 0;
            }
        }
    }
    else
    {
        ReleaseCount = 0;

        RawPriority = MotionDetect_GetEventPriority(RawEvent);
        StablePriority = MotionDetect_GetEventPriority(StableEvent);

        /* 允许从 NONE 进入事件，也允许向更高优先级事件升级 */
        if ((StableEvent == MOTION_EVENT_NONE) || (RawPriority >= StablePriority))
        {
            if (RawEvent == CandidateEvent)
            {
                CandidateCount++;
            }
            else
            {
                CandidateEvent = RawEvent;
                CandidateCount = 1;
            }

            NeedCount = MotionDetect_GetAssertCount(RawEvent);

            if (CandidateCount >= NeedCount)
            {
                StableEvent = RawEvent;
                CandidateEvent = MOTION_EVENT_NONE;
                CandidateCount = 0;
            }
        }
        else
        {
            /* 低优先级事件不把高优先级事件拉下来 */
            CandidateEvent = MOTION_EVENT_NONE;
            CandidateCount = 0;
        }
    }

    State->Event = StableEvent;
}