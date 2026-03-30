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

//Exit and entry thresholds
#define LC_MAX_BRAKE_THRESHOLD_PERCENT 0.05f
#define LC_MAX_INITIAL_SPEED_THRESHOLD_KPH 1.0f
#define LC_MIN_THROTTLE_THRESHOLD_PERCENT 0.90f

#define LC_MIN_FILTER_THRESHOLD_RPM 1000.0f //PLACEHOLDER

LaunchControl *LaunchControl_new(bool lcToggle) {
    LaunchControl* me = (LaunchControl*)malloc(sizeof(struct _LaunchControl));
    me->Kp = 10;
    me->Ki = 75;
    me->Kd = 0;
    me->pid = PID_new(me->Kp, me->Ki, me->Kd, 0.5, 1);
    me->slipRatioTarget = 0.2;
    me->lcToggle = lcToggle;
    me->currentSlipRatio = 0;
    me->currentVelocityDifference = 0;
    me->targetVelocityDifference = 0;
    me->lcTorqueCommand = 0;
    me->initialTorque = 75;
    me->k = 0.2;
    me->maxTorque = 231;
    me->prevTorque = me->initialTorque;
    me->lcSpeedCommand = 0;
    me->maxRPM = 7200;  // TODO: Determine max RPM for initial speed curve
    me->prevRPM = 0;
    me->commandMode = LC_COMMAND_TORQUE;
    me->useFilter = FALSE;
    me->mode = LC_MODE_SLIP_RATIO;
    me->state = LC_STATE_IDLE;
    me->phase = LC_PHASE_RAMP;
    return me;
}

void LaunchControl_reset(LaunchControl *me, MotorController *mcm) {
    me->state = LC_STATE_IDLE;
    me->phase = LC_PHASE_RAMP;
    me->lcTorqueCommand = 0;
    me->prevTorque = me->initialTorque;
    me->lcSpeedCommand = 0;
    me->prevRPM = 0;
    MCM_update_LC_torqueCommand(mcm, me->lcTorqueCommand);
    MCM_update_LC_speedCommand(mcm, me->lcSpeedCommand);
    //Always leave the MCM in torque mode when LC is not active
    MCM_updateSpeedModeStatus(mcm, FALSE);

    me->pid->totalError = 0;
    me->pid->previousError = 0;
}

void LaunchControl_updateState(LaunchControl *me, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm) {
    //Enters ready state if LC button is held, and the vehicle is near standstill and driver is not braking
    if (me->state == LC_STATE_IDLE && Sensor_LCButton.sensorValue == TRUE && 
        MCM_getGroundSpeedKPH(mcm) < LC_MAX_INITIAL_SPEED_THRESHOLD_KPH && bps->percent <= LC_MAX_BRAKE_THRESHOLD_PERCENT) {
        me->state = LC_STATE_READY;
    }
    //While in ready state, the driver should press throttle fully. Then, releasing the LC button will activate Launch Control
    if (me->state == LC_STATE_READY && Sensor_LCButton.sensorValue == FALSE) {
        if (tps->tps0_percent > LC_MIN_THROTTLE_THRESHOLD_PERCENT && bps->percent < LC_MAX_BRAKE_THRESHOLD_PERCENT) {
            me->state = LC_STATE_ACTIVE;
        }
        else {
            //When the LC button is released, if the driver is not pressing throttle fully or is braking, Launch Control is aborted
            me->state = LC_STATE_IDLE;
            LaunchControl_reset(me, mcm);
        }
    }
    //During active Launch Control, exit if driver lifts off throttle or brakes
    if (me->state == LC_STATE_ACTIVE && (tps->tps0_percent < LC_MIN_THROTTLE_THRESHOLD_PERCENT || bps->percent > LC_MAX_BRAKE_THRESHOLD_PERCENT)) {
        me->state = LC_STATE_IDLE;
        LaunchControl_reset(me, mcm);
    }
    MCM_update_LC_engagedStatus(mcm, (me->state != LC_STATE_IDLE));
}

void LaunchControl_updatePhase(LaunchControl *me, WheelSpeeds *wss) {
    //Use preset torque curve during intial part of launch when wheel speeds are not reading
    if (!WheelSpeeds_isWheelSpeedsNonZero(wss, me->useFilter)) {
        me->phase = LC_PHASE_RAMP;
        return;
    }

    if (me->phase == LC_PHASE_RAMP) {
        if (me->currentSlipRatio > 0.2) {
            me->phase = LC_PHASE_NONLINEAR;
        }
        else {
            me->phase = LC_PHASE_LINEAR;
        }
        return;
    }
    if (me->phase == LC_PHASE_NONLINEAR) {
        if (me->currentSlipRatio < 0.2) {
            me->phase = LC_PHASE_LINEAR;
            me->pid->totalError = 0;
        }
        return;
    }
    if (me->phase == LC_PHASE_LINEAR) {
        if (me->currentSlipRatio > 0.25) {
            me->phase = LC_PHASE_NONLINEAR;
        }
        return;
    }
}

void LaunchControl_updateSlipRatio(LaunchControl *me, WheelSpeeds *wss) {
    float fastestRearWheelsRPM = WheelSpeeds_getFastestRearRPM(wss, me->useFilter);
    float avgFrontWheelsRPM = WheelSpeeds_getGroundSpeedRPM(wss, 0, me->useFilter);
    if ((avgFrontWheelsRPM) != 0) {
        me->currentSlipRatio = (fastestRearWheelsRPM / avgFrontWheelsRPM) - 1.0f;
    }
}

void LaunchControl_updateVelocityDifference(LaunchControl *me, WheelSpeeds *wss) 
{
    float estimatedVehicleVelocity = WheelSpeeds_getGroundSpeedMPS(wss, 0, me->useFilter);
    me->currentVelocityDifference = WheelSpeeds_getFastestRearMPS(wss, me->useFilter) - estimatedVehicleVelocity;
    me->targetVelocityDifference = me->slipRatioTarget * estimatedVehicleVelocity;
}


void LaunchControl_applyTorqueCurve(LaunchControl *me, MotorController *mcm) {
    float torque = me->k * me->maxTorque + (1 - me->k) * me->prevTorque;
    me->lcTorqueCommand = (sbyte2) torque;
    me->prevTorque = me->lcTorqueCommand;
}

void LaunchControl_applySpeedCurve(LaunchControl *me, MotorController *mcm) {
    float rpm = me->k * me->maxRPM + (1 - me->k) * me->prevRPM;
    me->lcSpeedCommand = (sbyte2) rpm;
    me->prevRPM = (float)me->lcSpeedCommand;
}

void LaunchControl_calculateCommands(LaunchControl *me, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm, WheelSpeeds *wss)
{
    //Always monnitor slip ratio and velocity difference
    LaunchControl_updateSlipRatio(me, wss);
    LaunchControl_updateVelocityDifference(me, wss);

    //Exit if Launch Control is not enabled
    if (!me->lcToggle) { 
        return; 
    }

    LaunchControl_updateState(me, tps, bps, mcm);

    switch(me->state) {
        case LC_STATE_IDLE:
            //Do nothing
            break;

        case LC_STATE_READY:
            //During ready state, driver is able to press throttle without requesting any torque.
            //Reset both command values  
            me->lcTorqueCommand = 0;
            me->lcSpeedCommand = 0;
            MCM_update_LC_torqueCommand(mcm, me->lcTorqueCommand);
            MCM_update_LC_speedCommand(mcm, me->lcSpeedCommand);
            MCM_updateSpeedModeStatus(mcm, FALSE);
            break;
    

        case LC_STATE_ACTIVE:
            // Tell the MCM which command channel to use for this launch
            MCM_updateSpeedModeStatus(mcm, (me->commandMode == LC_COMMAND_SPEED));
            LaunchControl_updatePhase(me, wss);

            //if one WSS < 0, use speed curve 
            if (me->phase == LC_PHASE_RAMP) {
                //chooses curve based on mode
                if (me->commandMode == LC_COMMAND_SPEED) {
                    LaunchControl_applySpeedCurve(me, mcm);
                }
                else {
                    LaunchControl_applyTorqueCurve(me, mcm);
                }
            }
            //if all WSS > 0, use PID
            else { 
                LaunchControl_calculatePIDOutput(me);
                if (me->commandMode == LC_COMMAND_SPEED) {
                    me->lcSpeedCommand = (sbyte2)MCM_getMotorRPM(mcm) + (sbyte2)me->pid->output;
                }
                else if (me->commandMode == LC_COMMAND_TORQUE) {
                    me->lcTorqueCommand = (sbyte2)MCM_getCommandedTorque(mcm) + (sbyte2)me->pid->output;
                }
            }
            if (me->commandMode == LC_COMMAND_SPEED) {
				// TODO: change maxRPM
                if (me->lcSpeedCommand > (sbyte2)me->maxRPM) {
                    MCM_update_LC_speedCommand(mcm, (sbyte2)me->maxRPM);
                }
                else if (me->lcSpeedCommand < 0) {
                    MCM_update_LC_speedCommand(mcm, 0);
                }
                else {
                    MCM_update_LC_speedCommand(mcm, me->lcSpeedCommand);
                }
            }
            else if (me->commandMode == LC_COMMAND_TORQUE) {
                if (me->lcTorqueCommand > 220) {
                    MCM_update_LC_torqueCommand(mcm, 220);
                }
                else if (me->lcTorqueCommand < 0) {
                    MCM_update_LC_torqueCommand(mcm, 0);
                }
                else {
                    MCM_update_LC_torqueCommand(mcm, me->lcTorqueCommand);
                }
            }
            break;
    }
}
//end

void LaunchControl_updateFilterStatus(LaunchControl *me, MotorController *mcm) {
    if (MCM_getMotorRPM(mcm) > LC_MIN_FILTER_THRESHOLD_RPM) {
        me->useFilter = TRUE;
    }
    else {
        me->useFilter = FALSE;
    }
}

void LaunchControl_calculatePIDOutput(LaunchControl *me) 
{
    if (me->phase == LC_PHASE_NONLINEAR) {
        PID_updateGainValues(me->pid, me->Kp, 0, me->Kd); //Disable integral in non-linear phase
    }
    else {
        PID_updateGainValues(me->pid, me->Kp, me->Ki, me->Kd);
    }

    if (me->mode == LC_MODE_SLIP_RATIO) {
        PID_updateSetpoint(me->pid, me->slipRatioTarget);
        PID_computeOutput(me->pid, me->currentSlipRatio);
    }
    else if (me->mode == LC_MODE_VELOCITY_DIFFERENCE) {
        PID_updateSetpoint(me->pid, me->targetVelocityDifference);
        PID_computeOutput(me->pid, me->currentVelocityDifference);
    }

    if (me->pid->output > 20) {
        me->pid->output = 20;
    } 
    else if (me->pid->output < -20) {
        me->pid->output = -20;
    }
}


ubyte1 LaunchControl_getState(LaunchControl *me) { return me->state; }

ubyte1 LaunchControl_getPhase(LaunchControl *me) { return me->phase; }

sbyte2 LaunchControl_getTorqueCommand(LaunchControl *me) { return me->lcTorqueCommand; }

float LaunchControl_getSlipRatio(LaunchControl *me) { return me->currentSlipRatio; }

sbyte2 LaunchControl_getSlipRatioScaled(LaunchControl *me) { return (sbyte2)(me->currentSlipRatio * 1000.0f); }

sbyte2 LaunchControl_getVelocityDifferenceTarget(LaunchControl *me) { return me->targetVelocityDifference; }

sbyte2 LaunchControl_getCurrentVelocityDifference(LaunchControl *me) { return me->currentVelocityDifference; }

bool LaunchControl_getInitialCurveStatus(LaunchControl *me) { return me->phase == LC_PHASE_RAMP; }

bool LaunchControl_getActiveStatus(LaunchControl *me) { return me->state == LC_STATE_ACTIVE; }

bool LaunchControl_getFilterStatus(LaunchControl *me) { return me->useFilter; }

sbyte2 LaunchControl_getSpeedCommand(LaunchControl *me) { return me->lcSpeedCommand; }



