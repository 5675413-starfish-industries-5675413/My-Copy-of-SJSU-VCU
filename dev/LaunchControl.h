#ifndef _LAUNCHCONTROL_H
#define _LAUNCHCONTROL_H

#include "IO_Driver.h"
#include "wheelSpeeds.h"
#include "mathFunctions.h"
#include "initializations.h"
#include "sensors.h"
#include "torqueEncoder.h"
#include "brakePressureSensor.h"
#include "motorController.h"
#include "drs.h"
#include "PID.h"
#include "IO_Driver.h" //Includes datatypes, constants, etc - should be included in every c file


typedef enum {
    LC_MODE_SLIP_RATIO,
    LC_MODE_SLIP_DIFFERENCE
} LC_Mode;

typedef enum {
    LC_PHASE_RAMP,
    LC_PHASE_NONLINEAR,
    LC_PHASE_LINEAR,
} LC_Phase;

typedef enum {
    LC_IDLE,
    LC_READY,
    LC_ACTIVE
} LC_State;

typedef struct _LaunchControl {
    bool lcToggle;
    PID *pid;
    sbyte1 Kp;
    sbyte1 Ki;
    sbyte1 Kd;
    float currentSlipRatio;
    float slipRatioTarget;
    float currentVelocityDifference;
    float targetVelocityDifference;
    sbyte2 lcTorqueCommand;
    float initialTorque;
    float maxTorque;
    float prevTorque;
    float k;
    LC_State state;
    LC_Mode mode;
    LC_Phase phase;
} LaunchControl;

LaunchControl *LaunchControl_new(bool lcToggle);
void LaunchControl_reset(LaunchControl *me, MotorController *mcm);
void LaunchControl_updateState(LaunchControl *me, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm);
void LaunchControl_updateSlipRatio(LaunchControl *me, WheelSpeeds *wss);
void LaunchControl_applyTorqueCurve(LaunchControl *me, MotorController *mcm);
void LaunchControl_calculateCommands(LaunchControl *me, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm, WheelSpeeds *wss);
bool LaunchControl_isWheelSpeedsNonZero(WheelSpeeds *wss);
ubyte1 LaunchControl_getState(LaunchControl *me);
sbyte2 LaunchControl_getTorqueCommand(LaunchControl *me);
float LaunchControl_getSlipRatio(LaunchControl *me);
sbyte2 LaunchControl_getSlipRatioScaled(LaunchControl *me);
bool LaunchControl_getInitialCurveStatus(LaunchControl *me);
float LaunchControl_getPidOutput(LaunchControl *me);
void LaunchControl_calculatePIDOutput(LaunchControl *me);
void LaunchControl_updatePhase(LaunchControl *me, WheelSpeeds *wss);
ubyte1 LaunchControl_getPhase(LaunchControl *me);
void LaunchControl_updateSlipDifference(LaunchControl *me, WheelSpeeds *wss);

#endif