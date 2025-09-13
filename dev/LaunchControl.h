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


typedef struct _LaunchControl {

    sbyte2 lcTorqueCommand;

    float slipRatio;

    float maxTorque;
    float prevTorque;
    float k;


    ubyte4 safteyTimer;
    ubyte1 lcReady;
    ubyte1 lcActive;

    ubyte1 buttonDebug;
} LaunchControl;

LaunchControl *LaunchControl_new();
void LaunchControl_calculateSlipRatio(LaunchControl *me, MotorController *mcm, WheelSpeeds *wss);
void LaunchControl_calculateTorqueCommand(LaunchControl *me, MotorController *mcm);
void LaunchControl_checkState(LaunchControl *lc, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm, DRS *drs);

ubyte1 LaunchControl_getActiveStatus(LaunchControl *me);
ubyte1 LaunchControl_getReadyStatus(LaunchControl *me);

ubyte1 LaunchControl_getButtonDebug(LaunchControl *me);

#endif