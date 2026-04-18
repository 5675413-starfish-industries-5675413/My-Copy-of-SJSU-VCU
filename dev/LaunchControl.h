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

/*
 * PID controller that outputs a Torque REDUCTION (DNm).
 * All math is in float4 (decimal) per the VCU type rules. The controller
 * runs in the 10ms main loop (dt = 0.01s).
 */
typedef struct _PIDController {
    float4 kp;         /* Proportional gain */
    float4 ki;         /* Integral gain */
    float4 kd;         /* Derivative gain */
    float4 errorSum;   /* Anti-wound integral accumulator */
    float4 lastError;  /* For derivative term */
} PIDController;

/*
 * LaunchControl: computes a torque REDUCTION (in DNm) that is subtracted from
 * the driver's requested torque inside MCM_calculateCommands.
 * slipRatio = (Driven - Undriven) / Undriven     (rear-drive car)
 * Target slip is ~0.15 (15%).
 */
typedef struct _LaunchControl {
    float4 slipRatio;
    float4 targetSlip;          /* Slip ratio setpoint (0.15 = 15%) */
    sbyte2 lcTorqueReductionDNm; /* Torque to SUBTRACT from driver request (DNm) */
    bool   LCReady;
    bool   LCStatus;             /* For CAN debug */
    PIDController *pidController;
    ubyte1 potLC;

    ubyte1 buttonDebug;
} LaunchControl;

LaunchControl *LaunchControl_new(void);
void slipRatioCalculation(WheelSpeeds *wss, LaunchControl *lc);
void launchControlTorqueCalculation(LaunchControl *lc, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm);
bool getLaunchControlStatus(LaunchControl *lc);
sbyte2 getCalculatedTorque(void);
ubyte1 getButtonDebug(LaunchControl *lc);

#endif
