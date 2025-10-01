#include <stdlib.h>
#include <math.h>
#include "IO_RTC.h"
#include "IO_DIO.h"
#include "LaunchControl.h"
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
    me->pid = PID_new(200, 100, 0, 0.5, 1);
    PID_updateSetpoint(me->pid, 0.2);

    me->lcToggle = lcToggle;
    me->slipRatio = 0;
    me->lcTorqueCommand = 0;
    me->k = 0.05;
    me->maxTorque = 231;
    me->prevTorque = 0;
    me->isInitialCurve = FALSE;
    me->mode = FIRST_ORDER_ONLY;
    me->state = LC_IDLE;
    return me;
}

void LaunchControl_reset(LaunchControl *me) {
    me->state = LC_IDLE;
    me->isInitialCurve = FALSE;

    me->pid->totalError = 0;
    me->pid->previousError = 0;
}

void LaunchControl_updateState(LaunchControl *me, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm) 
{
    if (Sensor_LCButton.sensorValue == TRUE && MCM_getGroundSpeedKPH(mcm) < 1 && bps->percent < 0.05) {
        me->state = LC_READY;
    }

    if (me->state == LC_READY && Sensor_LCButton.sensorValue == FALSE && tps->tps0_percent > .90 && bps->percent < 0.05) {
        me->state = LC_ACTIVE;
    }
    else {
        me->state = LC_IDLE;
        LaunchControl_reset(me);
    }

    if (me->state == LC_ACTIVE && (tps->tps0_percent < .90 || bps->percent > 0.05)) {
        me->state = LC_IDLE;
        LaunchControl_reset(me);
    }
    MCM_update_LC_engagedStatus(mcm, (me->state != LC_IDLE));

}

void LaunchControl_calculateSlipRatio(LaunchControl *me, WheelSpeeds *wss)
{
    float avgFrontWheelsRPM = (WheelSpeeds_getWheelSpeedRPM(wss, FL, TRUE) + WheelSpeeds_getWheelSpeedRPM(wss, FR, TRUE)) / 2;
    float avgRearWheelsRPM = (WheelSpeeds_getWheelSpeedRPM(wss, RL, TRUE) + WheelSpeeds_getWheelSpeedRPM(wss, RR, TRUE)) / 2;
    float slipRatioCalc = ((avgRearWheelsRPM + 1) / (avgFrontWheelsRPM + 1)) - 1;
    me->slipRatio = slipRatioCalc;
}

void LaunchControl_calculateTorqueCurve(LaunchControl *me, MotorController *mcm) {
    float torque = me->k * me->maxTorque + (1 - me->k) * me->prevTorque;
    me->lcTorqueCommand = (sbyte2) torque;
    me->prevTorque = MCM_getCommandedTorque(mcm);
}

void LaunchControl_calculateCommands(LaunchControl *me, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm, WheelSpeeds *wss)
{
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
    
    if (me->mode == FIRST_ORDER_ONLY) {
       LaunchControl_calculateTorqueCurve(me, mcm);
       me->isInitialCurve = TRUE;
    }
    else if (me->mode == SLIP_CONTROLLER) {
        if (LaunchControl_isWheelSpeedsNonZero(wss)) {
            LaunchControl_calculateSlipRatio(me, wss);
            PID_computeOutput(me->pid, me->slipRatio);
            me->lcTorqueCommand = MCM_getCommandedTorque(mcm) + me->pid->output;
            me->isInitialCurve = FALSE;
        }
        else {
            LaunchControl_calculateTorqueCurve(me, mcm);
            me->isInitialCurve = TRUE;
        }
    }
    if (me->lcTorqueCommand < 0) {
        me->lcTorqueCommand = 0;
    }
    if (me->lcTorqueCommand > 231) {
        me->lcTorqueCommand = 231;
    }
    MCM_update_LC_torqueCommand(mcm, me->lcTorqueCommand);
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



LC_State LaunchControl_getState(LaunchControl *me) { return me->state; }

sbyte2 LaunchControl_getTorqueCommand(LaunchControl *me) { return me->lcTorqueCommand; }

float LaunchControl_getSlipRatio(LaunchControl *me) { return me->slipRatio; }

sbyte2 LaunchControl_getSlipRatioScaled(LaunchControl *me) { return (sbyte2)(me->slipRatio * 1000); }

bool LaunchControl_getInitialCurveStatus(LaunchControl *me) { return me->isInitialCurve; }

bool LaunchControl_getActiveStatus(LaunchControl *me) { return (me->state == LC_ACTIVE) ? TRUE : FALSE;  }

float LaunchControl_getPidOutput(LaunchControl *me) { return me->pid->output; }



