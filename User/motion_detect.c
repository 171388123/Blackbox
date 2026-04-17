#include "motion_detect.h"
#include <math.h>

#define RAD_TO_DEG_X10            572.9578f

#define TILT_THRESHOLD_X10        250      // 25.0°
#define MOVE_ACCEL_THRESHOLD      2000
#define MOVE_GYRO_THRESHOLD       3500
#define SHOCK_ACCEL_THRESHOLD     5000
#define SHOCK_GYRO_THRESHOLD      8000

static int16_t BaselineAx = 0;
static int16_t BaselineAy = 0;
static int16_t BaselineAz = 0;
static int16_t BaselinePitch_x10 = 0;
static int16_t BaselineRoll_x10 = 0;

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

void MotionDetect_SetBaseline(int16_t Ax, int16_t Ay, int16_t Az)
{
    BaselineAx = Ax;
    BaselineAy = Ay;
    BaselineAz = Az;

    BaselinePitch_x10 = MotionDetect_CalcPitch_x10(Ax, Ay, Az);
    BaselineRoll_x10  = MotionDetect_CalcRoll_x10(Ax, Ay, Az);
}

void MotionDetect_Update(MotionState_t *State,
                         int16_t Ax, int16_t Ay, int16_t Az,
                         int16_t Gx, int16_t Gy, int16_t Gz)
{
    uint16_t PitchDelta;
    uint16_t RollDelta;

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

    if ((State->AccelDelta > SHOCK_ACCEL_THRESHOLD) || (State->GyroAbsSum > SHOCK_GYRO_THRESHOLD))
    {
        State->Event = MOTION_EVENT_SHOCK;
    }
    else if ((PitchDelta > TILT_THRESHOLD_X10) || (RollDelta > TILT_THRESHOLD_X10))
    {
        State->Event = MOTION_EVENT_TILT;
    }
    else if ((State->AccelDelta > MOVE_ACCEL_THRESHOLD) || (State->GyroAbsSum > MOVE_GYRO_THRESHOLD))
    {
        State->Event = MOTION_EVENT_MOVE;
    }
    else
    {
        State->Event = MOTION_EVENT_NONE;
    }
}