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

LaunchControl *LaunchControl_new(bool lcToggle) 
{
    LaunchControl* lc = (LaunchControl*)malloc(sizeof(struct _LaunchControl));

    lc->lcToggle = lcToggle;

    lc->Kp = 10;
    lc->Ki = 75;
    lc->Kd = 0;
    lc->pid = PID_new(lc->Kp, lc->Ki, lc->Kd, 0, 1);
	
    lc->slipRatioTarget = 0.2;
    lc->currentSlipRatio = 0;
    lc->currentVelocityDifference = 0;
    lc->targetVelocityDifference = 0;

    lc->lcTorqueCommand = 0;
    lc->initialTorque = 75;
    lc->k = 0.2;
    lc->maxTorque = 231;
    lc->prevTorque = lc->initialTorque;

    lc->lcSpeedCommand = 0;
    lc->maxRPM = 7200;  // TODO: Determine max RPM for initial speed curve
	lc->initialRPM = 0;
    lc->prevRPM = lc->initialRPM;

    lc->commandMode = LC_COMMAND_TORQUE;
    lc->mode = LC_MODE_SLIP_RATIO;
    lc->state = LC_STATE_IDLE;
    lc->phase = LC_PHASE_RAMP;

	lc->useFilter = FALSE;

    return lc;
}

void LaunchControl_reset(LaunchControl *lc, MotorController *mcm) 
{
    lc->state = LC_STATE_IDLE;
    lc->phase = LC_PHASE_RAMP;

    lc->lcTorqueCommand = 0;
    lc->prevTorque = lc->initialTorque;
    MCM_update_LC_torqueCommand(mcm, lc->lcTorqueCommand);

    lc->lcSpeedCommand = 0;
    lc->prevRPM = lc->initialRPM;
    MCM_update_LC_speedCommand(mcm, lc->lcSpeedCommand);

    //Always leave the MCM in torque mode when LC is not active
    MCM_updateSpeedModeStatus(mcm, FALSE);

    lc->pid->totalError = 0;
	lc->pid->previousError = 0;
}

void LaunchControl_updateState(LaunchControl *lc, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm) 
{
    //Enters ready state if LC button is held, and the vehicle is near standstill and driver is not braking
    if (lc->state == LC_STATE_IDLE && Sensor_LCButton.sensorValue == TRUE && 
        MCM_getGroundSpeedKPH(mcm) < LC_MAX_INITIAL_SPEED_THRESHOLD_KPH && bps->percent <= LC_MAX_BRAKE_THRESHOLD_PERCENT) 
	{
        lc->state = LC_STATE_READY;
    }

    //While in ready state, the driver should press throttle fully. Then, releasing the LC button will activate Launch Control
    if (lc->state == LC_STATE_READY && Sensor_LCButton.sensorValue == FALSE) 
	{
        if (tps->tps0_percent > LC_MIN_THROTTLE_THRESHOLD_PERCENT && bps->percent < LC_MAX_BRAKE_THRESHOLD_PERCENT) 
		{
            lc->state = LC_STATE_ACTIVE;
        }
        else 
		{
            //When the LC button is released, if the driver is not pressing throttle fully or is braking, Launch Control is aborted
            lc->state = LC_STATE_IDLE;
            LaunchControl_reset(lc, mcm);
        }
    }

    //During active Launch Control, exit if driver lifts off throttle or brakes
    if (lc->state == LC_STATE_ACTIVE && (tps->tps0_percent < LC_MIN_THROTTLE_THRESHOLD_PERCENT || bps->percent > LC_MAX_BRAKE_THRESHOLD_PERCENT)) 
	{
        lc->state = LC_STATE_IDLE;
        LaunchControl_reset(lc, mcm);
    }

    MCM_update_LC_engagedStatus(mcm, (lc->state != LC_STATE_IDLE));
}

void LaunchControl_updatePhase(LaunchControl *lc, WheelSpeeds *wss) 
{
    //Use preset curve during intial part of launch when wheel speeds are not reading
    if (!WheelSpeeds_isDAQWheelSpeedsNonZero(wss)) 
	{
        lc->phase = LC_PHASE_RAMP;
        return;
    }

    if (lc->phase == LC_PHASE_RAMP)
	{
        if (lc->currentSlipRatio > 0.2) 
		{
            lc->phase = LC_PHASE_NONLINEAR;
        }
        else 
		{
            lc->phase = LC_PHASE_LINEAR;
        }
        return;
    }
    if (lc->phase == LC_PHASE_NONLINEAR) 
	{
        if (lc->currentSlipRatio < 0.2) 
		{
            lc->phase = LC_PHASE_LINEAR;
            lc->pid->totalError = 0;
        }
        return;
    }
    if (lc->phase == LC_PHASE_LINEAR) 
	{
        if (lc->currentSlipRatio > 0.25) 
		{
            lc->phase = LC_PHASE_NONLINEAR;
        }
        return;
    }
}

void LaunchControl_updateSlipRatio(LaunchControl *lc, WheelSpeeds *wss) 
{
	float4 avgRearWheelsRPM = (WheelSpeeds_getDAQWheelSpeedRPM(wss, RL) + WheelSpeeds_getDAQWheelSpeedRPM(wss, RR)) / 2;
	float4 avgFrontWheelsRPM = (WheelSpeeds_getDAQWheelSpeedRPM(wss, FL) + WheelSpeeds_getDAQWheelSpeedRPM(wss, FR)) / 2;
    if ((avgFrontWheelsRPM) != 0) 
	{
        lc->currentSlipRatio = (avgRearWheelsRPM / avgFrontWheelsRPM) - 1.0f;
    }
}

void LaunchControl_updateVelocityDifference(LaunchControl *lc, WheelSpeeds *wss) 
{
	// TODO: Change to DAQ wheel speeds
    float4 estimatedVehicleVelocity = WheelSpeeds_getGroundSpeedMPS(wss, 0, lc->useFilter);
    lc->currentVelocityDifference = WheelSpeeds_getFastestRearMPS(wss, lc->useFilter) - estimatedVehicleVelocity;
    lc->targetVelocityDifference = lc->slipRatioTarget * estimatedVehicleVelocity;
}


void LaunchControl_applyTorqueCurve(LaunchControl *lc, MotorController *mcm) 
{
    float4 torque = lc->k * lc->maxTorque + (1 - lc->k) * lc->prevTorque;
	lc->lcTorqueCommand = (sbyte2) torque;
    lc->prevTorque = lc->lcTorqueCommand;
}

void LaunchControl_applySpeedCurve(LaunchControl *lc, MotorController *mcm) 
{
    float4 rpm = lc->k * lc->maxRPM + (1 - lc->k) * lc->prevRPM;
    lc->lcSpeedCommand = (sbyte2) rpm;
    lc->prevRPM = (float4)lc->lcSpeedCommand;
}

void LaunchControl_calculateCommands(LaunchControl *lc, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm, WheelSpeeds *wss)
{
    //Always monnitor slip ratio and velocity difference
    LaunchControl_updateSlipRatio(lc, wss);
    LaunchControl_updateVelocityDifference(lc, wss);

    //Exit if Launch Control is not enabled
    if (!lc->lcToggle) 
	{ 
        return; 
    }

    LaunchControl_updateState(lc, tps, bps, mcm);

	if (lc->commandMode == LC_COMMAND_TORQUE) 
	{
		MCM_updateSpeedModeStatus(mcm, FALSE);
		LaunchControl_calculateTorqueCommand(lc, wss, mcm);
	}
	else if (lc->commandMode == LC_COMMAND_SPEED)
	{

		MCM_updateSpeedModeStatus(mcm, TRUE);
		LaunchControl_calculateSpeedCommand(lc, wss, mcm);
	}

}

void LaunchControl_calculateSpeedCommand(LaunchControl *lc, WheelSpeeds *wss, MotorController *mcm) 
{
    switch(lc->state) 
	{
    case LC_STATE_IDLE:
        //Do nothing
        break;

    case LC_STATE_READY:
		//During ready state, driver is able to press throttle without requesting any torque.
		lc->lcSpeedCommand = 0;
		MCM_update_LC_speedCommand(mcm, lc->lcSpeedCommand);
		break;
    
	case LC_STATE_ACTIVE:
		LaunchControl_updatePhase(lc, wss);

		if (lc->phase == LC_PHASE_RAMP) 
		{
			LaunchControl_applySpeedCurve(lc, mcm);
		}
		else 
		{
			LaunchControl_calculatePIDOutput(lc);
			lc->lcSpeedCommand = (sbyte2)MCM_getMotorRPM(mcm) + (sbyte2)lc->pid->output;
		}

		LaunchControl_updateMCMSpeedCommand(lc, mcm);
        break;
    }
}

void LaunchControl_updateMCMSpeedCommand(LaunchControl *lc, MotorController *mcm) 
{
	if (lc->lcSpeedCommand > 5000) 
	{
		MCM_update_LC_speedCommand(mcm, 5000);
	}
	else if (lc->lcSpeedCommand < 0) 
	{
		MCM_update_LC_speedCommand(mcm, 0);
	}
	else 
	{
		MCM_update_LC_speedCommand(mcm, lc->lcSpeedCommand);
	}
}

void LaunchControl_calculateTorqueCommand(LaunchControl *lc, WheelSpeeds *wss, MotorController *mcm) 
{
    switch(lc->state) 
	{
    case LC_STATE_IDLE:
		//Do nothing
		break;

	case LC_STATE_READY:
		//During ready state, driver is able to press throttle without requesting any speed
		lc->lcTorqueCommand = 0;
		MCM_update_LC_torqueCommand(mcm, lc->lcTorqueCommand);
		break;
    
	case LC_STATE_ACTIVE:
		LaunchControl_updatePhase(lc, wss);
		
		if (lc->phase == LC_PHASE_RAMP) 
		{
			LaunchControl_applyTorqueCurve(lc, mcm);
		}
		else 
		{ 
            LaunchControl_calculatePIDOutput(lc);
            lc->lcTorqueCommand = (sbyte2)MCM_getCommandedTorque(mcm) + (sbyte2)lc->pid->output;
        }

		LaunchControl_updateMCMTorqueCommand(lc, mcm);
        break;
    }
}

void LaunchControl_updateMCMTorqueCommand(LaunchControl *lc, MotorController *mcm) 
{
	if (lc->lcTorqueCommand > 220) 
	{
		MCM_update_LC_torqueCommand(mcm, 220);
	}
	else if (lc->lcTorqueCommand < 0) 
	{
		MCM_update_LC_torqueCommand(mcm, 0);
	}
	else 
	{
		MCM_update_LC_torqueCommand(mcm, lc->lcTorqueCommand);
	}
}

void LaunchControl_updateFilterStatus(LaunchControl *lc, MotorController *mcm) 
{
    if (MCM_getMotorRPM(mcm) > LC_MIN_FILTER_THRESHOLD_RPM) 
	{
        lc->useFilter = TRUE;
    }
    else 
	{
        lc->useFilter = FALSE;
    }
}

void LaunchControl_calculatePIDOutput(LaunchControl *lc) 
{
    if (lc->phase == LC_PHASE_NONLINEAR) 
	{
		//Disable integral in non-linear phase
        PID_updateGainValues(lc->pid, lc->Kp, 0, lc->Kd); 
    }
    else 
	{
        PID_updateGainValues(lc->pid, lc->Kp, lc->Ki, lc->Kd);
    }

    if (lc->mode == LC_MODE_SLIP_RATIO) 
	{
        PID_updateSetpoint(lc->pid, lc->slipRatioTarget);
        PID_computeOutput(lc->pid, lc->currentSlipRatio);
    }
    else if (lc->mode == LC_MODE_VELOCITY_DIFFERENCE) 
	{
        PID_updateSetpoint(lc->pid, lc->targetVelocityDifference);
        PID_computeOutput(lc->pid, lc->currentVelocityDifference);
    }
}


ubyte1 LaunchControl_getState(LaunchControl *me) { return me->state; }

ubyte1 LaunchControl_getPhase(LaunchControl *me) { return me->phase; }

sbyte2 LaunchControl_getTorqueCommand(LaunchControl *me) { return me->lcTorqueCommand; }

float4 LaunchControl_getSlipRatio(LaunchControl *me) { return me->currentSlipRatio; }

sbyte2 LaunchControl_getSlipRatioScaled(LaunchControl *me) { return (sbyte2)(me->currentSlipRatio * 1000.0f); }

sbyte2 LaunchControl_getVelocityDifferenceTarget(LaunchControl *me) { return me->targetVelocityDifference; }

sbyte2 LaunchControl_getCurrentVelocityDifference(LaunchControl *me) { return me->currentVelocityDifference; }

bool LaunchControl_getInitialCurveStatus(LaunchControl *me) { return me->phase == LC_PHASE_RAMP; }

bool LaunchControl_getActiveStatus(LaunchControl *me) { return me->state == LC_STATE_ACTIVE; }

bool LaunchControl_getFilterStatus(LaunchControl *me) { return me->useFilter; }

sbyte2 LaunchControl_getSpeedCommand(LaunchControl *me) { return me->lcSpeedCommand; }



