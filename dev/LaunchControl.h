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
    FIRST_ORDER_ONLY,
    SLIP_CONTROLLER
} LC_Mode;

typedef enum {
    LC_IDLE,
    LC_READY,
    LC_ACTIVE
} LC_State;

typedef struct _LaunchControl {
    bool lcToggle;
    PID *pid;
    float slipRatio;
    sbyte2 lcTorqueCommand;
    float maxTorque;
    float prevTorque;
    float k;
    bool isInitialCurve;
    LC_State state;
    LC_Mode mode;
} LaunchControl;

LaunchControl *LaunchControl_new(bool lcToggle);
void LaunchControl_reset(LaunchControl *me, MotorController *mcm);
void LaunchControl_updateState(LaunchControl *me, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm);
void LaunchControl_calculateSlipRatio(LaunchControl *me, WheelSpeeds *wss);
void LaunchControl_calculateTorqueCurve(LaunchControl *me, MotorController *mcm);
void LaunchControl_calculateCommands(LaunchControl *me, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm, WheelSpeeds *wss);
bool LaunchControl_isWheelSpeedsNonZero(WheelSpeeds *wss);
ubyte1 LaunchControl_getState(LaunchControl *me);
sbyte2 LaunchControl_getTorqueCommand(LaunchControl *me);
float LaunchControl_getSlipRatio(LaunchControl *me);
sbyte2 LaunchControl_getSlipRatioScaled(LaunchControl *me);
bool LaunchControl_getInitialCurveStatus(LaunchControl *me);
float LaunchControl_getPidOutput(LaunchControl *me);

#endif