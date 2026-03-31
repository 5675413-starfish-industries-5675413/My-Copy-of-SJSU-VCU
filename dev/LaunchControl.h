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
    LC_MODE_VELOCITY_DIFFERENCE
} LC_Mode;

typedef enum {
    LC_COMMAND_TORQUE,
    LC_COMMAND_SPEED
} LC_CommandMode;

typedef enum {
    LC_PHASE_RAMP,
    LC_PHASE_NONLINEAR,
    LC_PHASE_LINEAR,
} LC_Phase;

typedef enum {
    LC_STATE_IDLE,
    LC_STATE_READY,
    LC_STATE_ACTIVE
} LC_State;

typedef struct _LaunchControl {
    bool lcToggle;
    PID *pid;
    sbyte1 Kp;
    sbyte1 Ki;
    sbyte1 Kd;
    float4 currentSlipRatio;
    float4 slipRatioTarget;
    float4 currentVelocityDifference;
    float4 targetVelocityDifference;
    sbyte2 lcTorqueCommand;
    sbyte2 lcSpeedCommand; // rpm value
    float4 maxRPM; 
    float4 prevRPM;
    LC_CommandMode commandMode; 
    float4 initialTorque;
    float4 maxTorque;
    float4 prevTorque;
    float4 k;
    bool useFilter;
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
ubyte1 LaunchControl_getState(LaunchControl *me);
sbyte2 LaunchControl_getTorqueCommand(LaunchControl *me);
float4 LaunchControl_getSlipRatio(LaunchControl *me);
sbyte2 LaunchControl_getSlipRatioScaled(LaunchControl *me);
bool LaunchControl_getInitialCurveStatus(LaunchControl *me);
float4 LaunchControl_getPidOutput(LaunchControl *me);
void LaunchControl_calculatePIDOutput(LaunchControl *me);
void LaunchControl_updatePhase(LaunchControl *me, WheelSpeeds *wss);
ubyte1 LaunchControl_getPhase(LaunchControl *me);
void LaunchControl_updateSlipDifference(LaunchControl *me, WheelSpeeds *wss);
void LaunchControl_updateFilterStatus(LaunchControl *me, MotorController *mcm);
bool LaunchControl_getFilterStatus(LaunchControl *me);
sbyte2 LaunchControl_getVelocityDifferenceTarget(LaunchControl *me);
sbyte2 LaunchControl_getCurrentVelocityDifference(LaunchControl *me);
void LaunchControl_applySpeedCurve(LaunchControl *me, MotorController *mcm);
sbyte2 LaunchControl_getSpeedCommand(LaunchControl *me);
bool LaunchControl_getActiveStatus(LaunchControl *me);
void LaunchControl_updateVelocityDifference(LaunchControl *me, WheelSpeeds *wss);

#endif
