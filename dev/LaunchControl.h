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


typedef enum 
{
    LC_MODE_SLIP_RATIO,
    LC_MODE_VELOCITY_DIFFERENCE
} LC_Mode;

typedef enum 
{
    LC_COMMAND_TORQUE,
    LC_COMMAND_SPEED
} LC_CommandMode;

typedef enum 
{
    LC_PHASE_RAMP,
    LC_PHASE_NONLINEAR,
    LC_PHASE_LINEAR,
} LC_Phase;

typedef enum 
{
    LC_STATE_IDLE,
    LC_STATE_READY,
    LC_STATE_ACTIVE
} LC_State;

typedef struct _LaunchControl 
{
    bool lcToggle;

    PID *pid;
    sbyte1 Kp;
    sbyte1 Ki;
    sbyte1 Kd;

    float4 currentSlipRatio;
    float4 slipRatioTarget;
    float4 currentVelocityDifference;
    float4 targetVelocityDifference;

    float4 initialTorque;
    float4 maxTorque;
    float4 prevTorque;
    float4 k;
    sbyte2 lcTorqueCommand;

    float4 maxRPM; 
    float4 prevRPM;
	float4 initialRPM;
    sbyte2 lcSpeedCommand; 

    LC_CommandMode commandMode; 
    LC_State state;
    LC_Mode mode;
    LC_Phase phase;
	    
	bool useFilter;

} LaunchControl;

LaunchControl *LaunchControl_new(bool lcToggle);

void LaunchControl_reset(LaunchControl *lc, MotorController *mcm);

void LaunchControl_updateState(LaunchControl *lc, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm);
void LaunchControl_updatePhase(LaunchControl *lc, WheelSpeeds *wss);

void LaunchControl_updateSlipRatio(LaunchControl *lc, WheelSpeeds *wss);
void LaunchControl_updateVelocityDifference(LaunchControl *me, WheelSpeeds *wss);

void LaunchControl_calculateCommands(LaunchControl *lc, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm, WheelSpeeds *wss);
void LaunchControl_updateMCMTorqueCommand(LaunchControl *lc, MotorController *mcm);
void LaunchControl_calculateTorqueCommand(LaunchControl *lc, WheelSpeeds *wss, MotorController *mcm);
void LaunchControl_updateMCMSpeedCommand(LaunchControl *lc, MotorController *mcm);
void LaunchControl_calculateSpeedCommand(LaunchControl *lc, WheelSpeeds *wss, MotorController *mcm) ;

void LaunchControl_applyTorqueCurve(LaunchControl *lc, MotorController *mcm);
void LaunchControl_applySpeedCurve(LaunchControl *lc, MotorController *mcm);

void LaunchControl_calculatePIDOutput(LaunchControl *lc);

void LaunchControl_updateFilterStatus(LaunchControl *lc, MotorController *mcm);

bool LaunchControl_getActiveStatus(LaunchControl *lc);
bool LaunchControl_getFilterStatus(LaunchControl *lc);
bool LaunchControl_getInitialCurveStatus(LaunchControl *lc);

ubyte1 LaunchControl_getState(LaunchControl *lc);
ubyte1 LaunchControl_getPhase(LaunchControl *lc);

sbyte2 LaunchControl_getTorqueCommand(LaunchControl *lc);
sbyte2 LaunchControl_getSpeedCommand(LaunchControl *lc);

float4 LaunchControl_getSlipRatio(LaunchControl *lc);
sbyte2 LaunchControl_getSlipRatioScaled(LaunchControl *lc);

sbyte2 LaunchControl_getVelocityDifferenceTarget(LaunchControl *lc);
sbyte2 LaunchControl_getCurrentVelocityDifference(LaunchControl *lc);

float4 LaunchControl_getPidOutput(LaunchControl *me);

#endif
