#include <stdlib.h>
#include <math.h>
#include "IO_RTC.h"
#include "IO_DIO.h"
#include "LaunchControl.h"
#include "wheelSpeeds.h"
#include "mathFunctions.h"
#include "initializations.h"
#include "sensors.h"
#include "torqueEncoder.h"
#include "brakePressureSensor.h"
#include "motorController.h"
#include "sensorCalculations.h"
#include "drs.h"
extern Sensor Sensor_LCButton;




/* Start of Launch Control */
LaunchControl *LaunchControl_new(bool lcToggle)
{
    LaunchControl* me = (LaunchControl*)malloc(sizeof(struct _LaunchControl));
    me->Kp = 50;
    me->Ki = 20;
    me->Kd = 0;
    me->pid = PID_new(me->Kp, me->Ki, me->Kd, 0.5, 1);
    PID_updateSetpoint(me->pid, 0.2);

    me->lcToggle = lcToggle;
    me->slipRatio = 0;
    me->lcTorqueCommand = 0;
    me->initialTorque = 240;
    me->k = 0.6;
    me->maxTorque = 240;
    me->prevTorque = me->initialTorque;
    me->mode = SLIP_CONTROLLER;
    me->state = LC_IDLE;
    me->phase = LC_PHASE_RAMP;
    return me;
}

void LaunchControl_reset(LaunchControl *me, MotorController *mcm) {
    me->state = LC_IDLE;
    me->phase = LC_PHASE_RAMP;
    me->lcTorqueCommand = 0;
    me->prevTorque = me->initialTorque;
    MCM_update_LC_torqueCommand(mcm, me->lcTorqueCommand);

    me->pid->totalError = 0;
    me->pid->previousError = 0;
}

void LaunchControl_updateState(LaunchControl *me, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm) 
{
    if (me->state == LC_IDLE && Sensor_LCButton.sensorValue == TRUE && MCM_getGroundSpeedKPH(mcm) < 1 && bps->percent < 0.05) {
        me->state = LC_READY;
    }

    if (me->state == LC_READY && Sensor_LCButton.sensorValue == FALSE) {
        if (tps->tps0_percent > .90 && bps->percent < 0.05) {
            me->state = LC_ACTIVE;
        }
        else {
            me->state = LC_IDLE;
            LaunchControl_reset(me, mcm);
        }
    }

    if (me->state == LC_ACTIVE && (tps->tps0_percent < .90 || bps->percent > 0.05)) {
        me->state = LC_IDLE;
        LaunchControl_reset(me, mcm);
    }
    MCM_update_LC_engagedStatus(mcm, (me->state != LC_IDLE));
    
}

void LaunchControl_updatePhase(LaunchControl *me, WheelSpeeds *wss) {
    if (!LaunchControl_isWheelSpeedsNonZero(wss)) {
        me->phase = LC_PHASE_RAMP;
        return;
    }

    if (me->phase == LC_PHASE_NONLINEAR) {
        if (me->slipRatio < 0.2) {
            me->phase = LC_PHASE_LINEAR;
            me->pid->totalError = 0;
        }
    }
    else if (me->phase == LC_PHASE_LINEAR) {
        if (me->slipRatio > 0.25) {
            me->phase = LC_PHASE_NONLINEAR;
        }
    }
    else if (me->phase == LC_PHASE_RAMP) {
        if (me->slipRatio > 0.2) {
            me->phase = LC_PHASE_NONLINEAR;
        }
        else {
            me->phase = LC_PHASE_LINEAR;
        }
    }
}

void LaunchControl_calculateSlipRatio(LaunchControl *me, WheelSpeeds *wss)
{
    float avgFrontWheelsRPM = ((WheelSpeeds_getWheelSpeedRPM(wss, FL, TRUE) + 0.5) + (WheelSpeeds_getWheelSpeedRPM(wss, FR, TRUE) + 0.5)) / 2;
    float avgRearWheelsRPM = ((WheelSpeeds_getWheelSpeedRPM(wss, RL, TRUE) + 0.5) + (WheelSpeeds_getWheelSpeedRPM(wss, RR, TRUE) + 0.5)) / 2;
    if ((avgFrontWheelsRPM) != 0) {
        me->slipRatio = (avgRearWheelsRPM / avgFrontWheelsRPM) - 1;
    }
}

void LaunchControl_calculateTorqueCurve(LaunchControl *me, MotorController *mcm) {
    float torque = me->k * me->maxTorque + (1 - me->k) * me->prevTorque;
    me->lcTorqueCommand = (sbyte2) torque;
    me->prevTorque = me->lcTorqueCommand;
}

void LaunchControl_calculateCommands(LaunchControl *me, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm, WheelSpeeds *wss)
{
    LaunchControl_calculateSlipRatio(me, wss);

    if(!me->lcToggle) {
        return;
    }

    LaunchControl_updateState(me, tps, bps, mcm);
    
    if (me->state == LC_READY) {
        me->lcTorqueCommand = 0;
        MCM_update_LC_torqueCommand(mcm, me->lcTorqueCommand);
        return;
    }
    if (me->state != LC_ACTIVE) {
        return;
    }

    LaunchControl_updatePhase(me, wss);
    
    if (me->mode == SLIP_CONTROLLER) {
        if (me->phase == LC_PHASE_RAMP) {
            LaunchControl_calculateTorqueCurve(me, mcm);
        }
        else {
            LaunchControl_calculatePIDOutput(me);
            me->lcTorqueCommand = MCM_getFeedbackTorque(mcm) + me->pid->output;
        }
    }
    MCM_update_LC_torqueCommand(mcm, me->lcTorqueCommand);
}

void LaunchControl_calculatePIDOutput(LaunchControl *me) 
{
    if (me->phase == LC_PHASE_NONLINEAR) {
        PID_updateGainValues(me->pid, me->Kp, 0, me->Kd); //Disable integral in non-linear phase
    }
    else {
        PID_updateGainValues(me->pid, me->Kp, me->Ki, me->Kd);
    }

    PID_computeOutput(me->pid, me->slipRatio);
    if (me->pid->output > 20) {
        me->pid->output = 20;
    } 
    else if (me->pid->output < -20) {
        me->pid->output = -20;
    }

}

//maybe put this in wheelSpeeds.c ?
bool LaunchControl_isWheelSpeedsNonZero(WheelSpeeds *wss) {
    if (WheelSpeeds_getWheelSpeedRPM(wss, FL, TRUE) != 0 
        && WheelSpeeds_getWheelSpeedRPM(wss, FR, TRUE) != 0 
        && WheelSpeeds_getWheelSpeedRPM(wss, RL, TRUE) != 0 
        && WheelSpeeds_getWheelSpeedRPM(wss, RR, TRUE) != 0) {
            return TRUE;
        }
    return FALSE;
}



ubyte1 LaunchControl_getState(LaunchControl *me) { return me->state; }

ubyte1 LaunchControl_getPhase(LaunchControl *me) { return me->phase; }

sbyte2 LaunchControl_getTorqueCommand(LaunchControl *me) { return me->lcTorqueCommand; }

float LaunchControl_getSlipRatio(LaunchControl *me) { return me->slipRatio; }

sbyte2 LaunchControl_getSlipRatioScaled(LaunchControl *me) { return (sbyte2)(me->slipRatio * 1000.0f); }

bool LaunchControl_getInitialCurveStatus(LaunchControl *me) { return (me->phase == LC_PHASE_RAMP) ? TRUE : FALSE; }

bool LaunchControl_getActiveStatus(LaunchControl *me) { return (me->state == LC_ACTIVE) ? TRUE : FALSE;  }

float LaunchControl_getPidOutput(LaunchControl *me) { return me->pid->output; }



