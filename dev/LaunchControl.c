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
float Calctorque;

/* Start of Launch Control */
LaunchControl *LaunchControl_new(){
    LaunchControl* me = (LaunchControl*)malloc(sizeof(struct _LaunchControl));
    // malloc returns NULL if it fails to allocate memory
    if (me == NULL)
        return NULL;

    me->lcTorqueCommand = NULL;

    me->slipRatio = 0;
    
    me->maxTorque = 231;
    me->prevTorque = 0;
    me->k = 0.2;

    me->safteyTimer = 0;
    me->lcReady = FALSE;
    me->lcActive = FALSE;

    me->buttonDebug = 0; // This exists as a holdover piece of code to what I presume is debugging which button was which on the steering wheel. should remove / place elsewhere

    return me;
}

void LaunchControl_calculateTorqueCommand(LaunchControl *me,MotorController *mcm) {
    if (MCM_get_LC_activeStatus(mcm)) {
        float torque = me->k * me->maxTorque + (1- me->k) * me->prevTorque;
        me->lcTorqueCommand = (sbyte2) torque;
        me->prevTorque = me->lcTorqueCommand;
        MCM_update_LC_torqueCommand(mcm, me->lcTorqueCommand);
    }
}
void LaunchControl_calculateSlipRatio(LaunchControl *me, MotorController *mcm, WheelSpeeds *wss){
    //Non interpolated Wheelspeeds always read 0 for some reason, so we are forced to use the interpolated speeds.
    if( (ubyte2)(WheelSpeeds_getWheelSpeedRPM(wss, FL, TRUE) + 0.5) != 0 ) {
        float RearR = (WheelSpeeds_getWheelSpeedRPM(wss, RR, TRUE) + 0.5);
        float FrontL = (WheelSpeeds_getWheelSpeedRPM(wss, FL, TRUE) + 0.5);
        float calcs = (RearR/ FrontL)-1;
        me->slipRatio = calcs;
    }
}

void LaunchControl_checkState(LaunchControl *me, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm, DRS *drs) {
    sbyte2 speedKph = MCM_getGroundSpeedKPH(mcm);

    /* LC STATUS CONDITIONS *//*
     * lcReady = FALSE && lcActive = FALSE -> NOTHING HAPPENS
     * lcReady = TRUE  && lcActive = FALSE -> We are in the prep stage for lc, and all entry conditions for being in prep stage have and continue to be monitored
     * lcReady = FALSE && lcActive = TRUE  -> We have left the prep stage by releasing the lc button on the steering wheel, stay in Launch until exit conditions are met
     * AT ALL TIMES, EXIT CONDITIONS ARE CHECKED FOR BOTH STATES
    */

    /**
     * Launch Control Pre-Staging Operations:
     * If the car is near 0 kph (in case of wss float issues) & the Launch Button is pressed,
     * we initialise a 0.5 second timer to confirm a valid Launch attempt
     * Once this timer reaches maturity, we are now in "ready" state
     * The driver can now fully press TPS/APPS without moving car
     * 
     * Upon button release, we are now in "active" state and will proceed with our launch as intended -> car go eeeeeeeee (e-motor sounds)
     * 
     * At any time, an exit condition can be triggered to reset this staging operation and cancel our launch attempt
     */
    // Handle the LC Active Entry first so we don't need to displace the lcReady = FALSE trigger to a differnet place (if a button is released, lcReady would otherwise report false before we try to enter lcActive)
    if( me->lcReady == TRUE && Sensor_LCButton.sensorValue == FALSE ){
        me->lcActive = TRUE;
        me->lcReady = FALSE;
    }
    
    if(Sensor_LCButton.sensorValue == TRUE && speedKph < 1 && bps->percent < 0.05 ) {
        if (me->safteyTimer == 0){
            IO_RTC_StartTime(&me->safteyTimer);
        }
        else if (IO_RTC_GetTimeUS(me->safteyTimer) >= 50000) {
            me->lcReady = TRUE;
            me->safteyTimer = 0; // We don't need to track the time anymore
        }
    } else { me->lcReady = FALSE; }

    if( tps->tps0_percent < 0.90 || tps->tps0_percent < 0.90 || bps->percent > 0.05 ){
        me->lcActive = FALSE;
        me->lcTorqueCommand = NULL;
    }
    
    MCM_update_LC_activeStatus(mcm, (bool)me->lcActive);
    MCM_update_LC_readyStatus(mcm, (bool)me->lcReady);
}

sbyte2 LaunchControl_getTorqueCommand(LaunchControl *me){ return me->lcTorqueCommand; }

float LaunchControl_getSlipRatio(LaunchControl *me){ return me->slipRatio; }

ubyte1 LaunchControl_getActiveStatus(LaunchControl *me){ return me->lcActive; }

ubyte1 LaunchControl_getReadyStatus(LaunchControl *me){ return me->lcReady; }

ubyte1 LaunchControl_getButtonDebug(LaunchControl *me) { return me->buttonDebug; }




