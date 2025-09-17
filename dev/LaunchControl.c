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
extern Sensor Sensor_DRSKnob;

static const sbyte2 Low = 105;
static const sbyte2 Standard = 110;
static const sbyte2 Medium_High = 115;
static const sbyte2 High = 120;
static const sbyte2 Rain_Guess_1 = 40;
static const sbyte2 Rain_Guess_2 = 50;

/* Start of Launch Control */
LaunchControl *LaunchControl_new(bool toggle)
{
    LaunchControl* me = (LaunchControl*)malloc(sizeof(struct _LaunchControl));
    // malloc returns NULL if it fails to allocate memory
    if (me == NULL)
        return NULL;

    me->pid = PID_new(200, 100, 0, 0.5, 1);
    PID_updateSetpoint(me->pid, 0.2);

    me->lcTorqueCommand = NULL;
    me->toggle = toggle;
    
    me->fakeLCButtonStatus = FALSE;
    me->slipRatio = 0;
    me->slipRatioThreeDigits = 0;

    me->maxTorque = 231;
    me->prevTorque = 0;
    me->k = 0.05;

    me->initialCurve = FALSE;
    me->initialTorque = Standard;
    me->constA = 7;
    me->constB = 20;

    me->lcReady = FALSE;
    me->lcActive = FALSE;

    me->buttonDebug = 0; // This exists as a holdover piece of code to what I presume is debugging which button was which on the steering wheel. should remove / place elsewhere

    return me;
}


void LaunchControl_initialTorqueCurve(LaunchControl* me, MotorController* mcm)
{
    me->lcTorqueCommand = (sbyte2) me->initialTorque + ( MCM_getMotorRPM(mcm) * me->constA / me->constB ); // Tunable Values will be the inital Torque Request @ 0 and the scalar factor
}

void LaunchControl_calculateTorqueCommandPID(LaunchControl *me,MotorController *mcm) 
{
    sbyte2 speedKph = MCM_getGroundSpeedKPH(mcm);
    if ( MCM_getGroundSpeedKPH(mcm) < 5 ) 
    {
        me->initialCurve = TRUE;
        LaunchControl_initialTorqueCurve(me, mcm);
    } 
    else 
    {
        me->initialCurve = FALSE;
        PID_computeOutput(me->pid, me->slipRatio);
        me->lcTorqueCommand = MCM_getCommandedTorque(mcm) + me->pid->output;
    }

    if(MCM_getMotorRPM(mcm) >= 3500) 
    {
        me->lcTorqueCommand = 225; // Mini-Torque Derate, might help with field weakening as well as staying under 80 kwh threshold
    }

    MCM_update_LC_torqueCommand(mcm, me->lcTorqueCommand);
}

void LaunchControl_calculateTorqueCommand(LaunchControl *me,MotorController *mcm) 
{
    float torque = me->k * me->maxTorque + (1- me->k) * me->prevTorque;
    me->lcTorqueCommand = (sbyte2) torque;
    me->prevTorque = me->lcTorqueCommand;
    MCM_update_LC_torqueCommand(mcm, me->lcTorqueCommand);
}

void LaunchControl_calculateSlipRatio(LaunchControl *me, MotorController *mcm, WheelSpeeds *wss)
{
    //Non interpolated Wheelspeeds always read 0 for some reason, so we are forced to use the interpolated speeds.
    if( (ubyte2)(WheelSpeeds_getWheelSpeedRPM(wss, FL, TRUE) + 0.5) != 0 ) {
        float RearR = (WheelSpeeds_getWheelSpeedRPM(wss, RR, TRUE) + 0.5);
        float FrontL = (WheelSpeeds_getWheelSpeedRPM(wss, FL, TRUE) + 0.5);
        float calcs = (RearR/ FrontL)-1;
        ubyte4 threeDigitCalcs = (RearR * 1000) / FrontL;
        me->slipRatioThreeDigits = (ubyte2) threeDigitCalcs;

        me->slipRatio = calcs;
    }
}

void LaunchControl_checkState(LaunchControl *me, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm) 
{
    /* LC STATUS CONDITIONS *//*
     * lcReady = FALSE && lcActive = FALSE -> NOTHING HAPPENS
     * lcReady = TRUE  && lcActive = FALSE -> We are in the prep stage for lc, and all entry conditions for being in prep stage have and continue to be monitored
     * lcReady = FALSE && lcActive = TRUE  -> We have left the prep stage by releasing the lc button on the steering wheel, stay in Launch until exit conditions are met
     * AT ALL TIMES, EXIT CONDITIONS ARE CHECKED FOR BOTH STATES
    */

    sbyte2 speedKph = MCM_getGroundSpeedKPH(mcm);

    if (me->lcReady == TRUE && me->fakeLCButtonStatus == FALSE)
    {
        me->lcActive = TRUE;
        me->lcReady = FALSE;
    }
    
    if(me->fakeLCButtonStatus == TRUE && speedKph < 1 && bps->percent < 0.05 ) 
    {
        me->lcReady = TRUE;
    }
    else 
    {
        me->lcReady = FALSE;
    }

    if (tps->tps0_percent < 0.90 || tps->tps1_percent < 0.90 || bps->percent > 0.05) 
    {
        me->lcActive = FALSE;
        me->lcTorqueCommand = NULL;
    }
    
    MCM_update_LC_activeStatus(mcm, me->lcActive);
    MCM_update_LC_readyStatus(mcm, me->lcReady);
}

void LaunchControl_calculateCommands(LaunchControl *me, MotorController *mcm, WheelSpeeds *wss, TorqueEncoder *tps, BrakePressureSensor *bps) 
{
    if(LaunchControl_getToggleStatus(me)) 
    {
        LaunchControl_checkState(me,tps,bps,mcm);
        if (MCM_get_LC_activeStatus(mcm)) 
        {
            LaunchControl_calculateSlipRatio(me, mcm, wss);
            LaunchControl_calculateTorqueCommand(me, mcm);
        }
    }
}

void LaunchControl_calculateCommandsPID(LaunchControl *me, MotorController *mcm, WheelSpeeds *wss, TorqueEncoder *tps, BrakePressureSensor *bps) 
{
    if(LaunchControl_getToggleStatus(me)) 
    {
        LaunchControl_checkState(me,tps,bps,mcm);
        if (MCM_get_LC_activeStatus(mcm)) 
        {
            LaunchControl_calculateSlipRatio(me, mcm, wss);
            LaunchControl_calculateTorqueCommandPID(me, mcm);
        }
    }
}

sbyte2 LaunchControl_getTorqueCommand(LaunchControl *me) { return me->lcTorqueCommand; }

float LaunchControl_getSlipRatio(LaunchControl *me) { return me->slipRatio; }

ubyte2 LaunchControl_getSlipRatioThreeDigits(LaunchControl *me) { return me->slipRatioThreeDigits; }

bool LaunchControl_getActiveStatus(LaunchControl *me) { return me->lcActive; }

bool LaunchControl_getReadyStatus(LaunchControl *me) { return me->lcReady; }

float LaunchControl_getInitialCurveStatus(LaunchControl *me) { return me->initialCurve; }

float LaunchControl_getPidOutput(LaunchControl *me) { return me->pid->output; }

bool LaunchControl_getToggleStatus(LaunchControl *me) { return me->toggle; }


ubyte1 LaunchControl_getButtonDebug(LaunchControl *me) { return me->buttonDebug; }





