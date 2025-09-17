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



typedef struct _LaunchControl {

    PID *pid;

    bool toggle;
    bool fakeLCButtonStatus;
    float slipRatio;
    ubyte2 slipRatioThreeDigits;

    sbyte2 lcTorqueCommand;
    float maxTorque;
    float prevTorque;
    float k;

    bool initialCurve;
    sbyte2 initialTorque;
    sbyte2 constA;
    sbyte2 constB;

    bool lcReady;
    bool lcActive;

    ubyte1 buttonDebug;
} LaunchControl;

LaunchControl *LaunchControl_new(bool toggle);
void LaunchControl_calculateSlipRatio(LaunchControl *me, MotorController *mcm, WheelSpeeds *wss);
void LaunchControl_calculateTorqueCommand(LaunchControl *me, MotorController *mcm);
void LaunchControl_checkState(LaunchControl *lc, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm);
void LaunchControl_calculateCommands(LaunchControl *me, MotorController *mcm, WheelSpeeds *wss, TorqueEncoder *tps, BrakePressureSensor *bps);

sbyte2 LaunchControl_getTorqueCommand(LaunchControl *me);
float LaunchControl_getSlipRatio(LaunchControl *me);
ubyte2 LaunchControl_getSlipRatioThreeDigits(LaunchControl *me);

bool LaunchControl_getActiveStatus(LaunchControl *me);
bool LaunchControl_getReadyStatus(LaunchControl *me);
bool LaunchControl_getToggleStatus(LaunchControl *me);

ubyte1 LaunchControl_getButtonDebug(LaunchControl *me);

float LaunchControl_getInitialCurveStatus(LaunchControl *me);
void LaunchControl_calculateTorqueCommandPID(LaunchControl *me,MotorController *mcm);
void LaunchControl_initialTorqueCurve(LaunchControl* me, MotorController* mcm);
float LaunchControl_getPidOutput(LaunchControl *me);

#endif