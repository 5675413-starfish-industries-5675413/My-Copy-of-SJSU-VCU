/*****************************************************************************
 * powerLimit.c - Power Limiting using a PID controller to simplify calculations
 * Previous Author(s): Harleen Sandhu/Shaun Gilmore 
 * Current Author(s): Akash Karthik
 ******************************************************************************
 * Power Limiting code with a flexible Power Target & Initialization Limit
 * Goal: Find a way to limit power under a certain KWH limit (80kwh) while maximizing torque
 * Methods: Currently we are ONLY using the Torque PID method
 ****************************************************************************/
#include "IO_Driver.h" //Includes datatypes, constants, etc - should be included in every c file
#include "motorController.h"
#include "PID.h"
#include "powerLimit.h"
#include "mathFunctions.h"

#ifndef POWERLIMITCONSTANTS
#define POWERLIMITCONSTANTS

#endif

// #define ELIMINATE_CAN_MESSAGES
/** PARAMETERS **/
PowerLimit* POWERLIMIT_new(bool plToggle){
    PowerLimit* me = (PowerLimit*)malloc(sizeof(PowerLimit));
    me->plToggle=plToggle;
    me->pid = PID_new(10, 50, 0, 231,10); // last value tells you the factor the PID gets divided by
    me->plMode = 1; // 1 = Torque PID
    me->plStatus = FALSE; // FALSE = Off, TRUE = On
    me->plTorqueCommand = 0; // Torque command in deciNewton-meters
    me->plTargetPower = 30;// HERE IS WHERE YOU CHANGE POWERLIMIT (units = kW)
    me->plThresholdDiscrepancy = 15; // Threshold discrepancy in kW
    me->plInitializationThreshold = 0; // Initialization threshold in kW
    me->clampingMethod = 6; // Clamping method
    me->plAlwaysOn = TRUE; // TRUE = if is above threshold then stay on even if power is below threshold, FALSE = Only on if power is above threshold
    return me;
}

void PowerLimit_setPLInitializationThreshold(PowerLimit* me){
    me->plInitializationThreshold = me->plTargetPower - me->plThresholdDiscrepancy;
}

/** COMPUTATIONS **/

void PowerLimit_calculateCommand(PowerLimit *me, MotorController *mcm, TorqueEncoder *tps){
    PowerLimit_setPLInitializationThreshold(me);
    if (me->plStatus == FALSE){
        me->pid->totalError = 0;
        me->pid->proportional = 0;
        me->pid->integral = 0;
    }
    if (!me->plToggle || MCM_commands_getAppsTorque(mcm) == 0) { // if no torque command, turn off PL
        me->plStatus = FALSE;
    }
    else {
        sbyte4 current_power_kw = (sbyte4) ((MCM_getDCVoltage(mcm) * MCM_getDCCurrent(mcm)) / 1000); //manually calculated bc getPower dont work
        if (current_power_kw <= 0){
            me->plStatus = FALSE;
        }
        else{
            if (me->plAlwaysOn==FALSE) { // if PL is not always on, only turn on if power is above threshold and off if below
                if (current_power_kw > me->plInitializationThreshold) {
                    me->plStatus = TRUE;
                } else {
                    me->plStatus = FALSE;
                }
            } else { // if PL is always on, only turn on if power is above threshold and never turn off
                if (me->plStatus==FALSE && current_power_kw > me->plInitializationThreshold) {
                    me->plStatus = TRUE;
                }
            }
        }
    }

  
//1.TQ equation only

if (me->plStatus){
    if(me->plMode==1){
        POWERLIMIT_TorquePID(me, mcm);
    }
    if(me->plMode==2){
        POWERLIMIT_PowerPID(me,mcm);
    }
    }
else{
    MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand);
    MCM_set_PL_updateStatus(mcm, me->plStatus);
}
}

void POWERLIMIT_TorquePID(PowerLimit *me, MotorController *mcm){
    me->plMode = 1;
    PID_setSaturationPoint(me->pid, 231);

    sbyte4 motorRPM   = MCM_getMotorRPM(mcm);
    if (motorRPM == 0){
        motorRPM = 1; //avoid division by 0
    }
    sbyte2 commandedTorque = (sbyte2)MCM_getCommandedTorque(mcm);
    float torqueSetpoint = (me->plTargetPower - (2.0))* (9549.0/motorRPM);
    float torqueSetpointFloat = (float)(torqueSetpoint);

    // current safety check
    sbyte4 dcVoltage = MCM_getDCVoltage(mcm);
    sbyte4 dcCurrent = MCM_getDCCurrent(mcm);
    if (dcCurrent < 0){
        dcCurrent = 0;
    }
    POWERLIMIT_updatePIDController(me, torqueSetpointFloat, commandedTorque);
    float pidOutput = PID_getOutput(me->pid);
    me->plTorqueCommand = (sbyte2)((pidOutput + commandedTorque) * 10);
    if (me->plTorqueCommand > 2310) {
         me->plTorqueCommand = 2310; //saturation point in deciNewton-meters
     }
     MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand);
     MCM_set_PL_updateStatus(mcm, me->plStatus);
}

void POWERLIMIT_PowerPID(PowerLimit *me, MotorController *mcm){
    me->plMode = 2;
    me->pid->saturationValue = 80000; ////////////

    sbyte2 commandedTQ = MCM_getCommandedTorque(mcm);
    sbyte4 motorRPM = MCM_getMotorRPM(mcm);
    sbyte4 dcVoltage = MCM_getDCVoltage(mcm);
    sbyte4 dcCurrent = MCM_getDCCurrent(mcm);
    

    float setpointPower =40000.0f;
    float drawnPower = (float)((dcVoltage*dcCurrent)*1.0);

    
    POWERLIMIT_updatePIDController(me, setpointPower, drawnPower);

    float pidOutput = me->pid->output;

    me->plTorqueCommand = (sbyte2)(((pidOutput + drawnPower) * (9.549 / motorRPM)) * 10.0);    

    if (me->plTorqueCommand > 2310) {
            me->plTorqueCommand = 2310; //saturation point in deciNewton-meters
        }
    if (me->plTorqueCommand > commandedTQ) {
            me->plTorqueCommand = commandedTQ;
        }
    
    MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand); ///// set max to be based off of TPS reading and skip middle man, commanded TQ from MCM
    MCM_set_PL_updateStatus(mcm, me->plStatus);
}


void POWERLIMIT_updatePIDController(PowerLimit* me, float pidSetpoint, float sensorValue) {
        float currentError = pidSetpoint - sensorValue;
        switch (me->clampingMethod) {
            case 0: //No clamping
                me->pid->antiWindupFlag = FALSE;
                break;
            case 1: 
                if (currentError > 0) {
                    me->pid->antiWindupFlag = TRUE;
                    PID_setSaturationPoint(me->pid, 231); 
                }
                else {
                    me->pid->antiWindupFlag = FALSE;
                }
                break;
            case 2: 
                if (currentError > 0) {
                    me->pid->antiWindupFlag = TRUE;
                    PID_setSaturationPoint(me->pid, sensorValue); 
                }
                else {
                    me->pid->antiWindupFlag = FALSE;
                }
                break;
            case 3: 
                if (currentError < 0) {
                    me->pid->antiWindupFlag = TRUE;
                    me->pid->totalError -= PID_getPreviousError(me->pid); 
                }
                else {
                    me->pid->antiWindupFlag = FALSE;
                }
                break;
            case 4: 
                if (currentError > 0) {
                    me->pid->antiWindupFlag = TRUE;
                    pidSetpoint = sensorValue; 
                }
                else {
                    me->pid->antiWindupFlag = FALSE;
                }
                break;
            case 5: {
                float tentativeOutput = me->pid->proportional + me->pid->integral + me->pid->derivative + sensorValue;
                if (tentativeOutput > me->pid->saturationValue) {
                    me->pid->antiWindupFlag = TRUE;
                    float correction = me->pid->saturationValue - tentativeOutput;
                    float scaledCorrection = correction * me->pid->Ki;
                    me->pid->integral += scaledCorrection;
                }
                else {
                    me->pid->antiWindupFlag = FALSE;
                }
                break;
            }
            case 6: {
                if (me->pid->proportional + me->pid->integral + me->pid->derivative + sensorValue > me->pid->saturationValue) {
                    me->pid->totalError -= currentError;
                    me->pid->antiWindupFlag = TRUE;
                }
                else {
                    me->pid->antiWindupFlag = FALSE;
                }
                break;
            }
        }
        PID_updateSetpoint(me->pid, pidSetpoint);
        PID_computeOutput(me->pid, sensorValue);
}



ubyte1 POWERLIMIT_getClampingMethod(PowerLimit* me){
    return me->clampingMethod;
}

bool POWERLIMIT_getStatus(PowerLimit* me){
    return me->plStatus;
}

ubyte1 POWERLIMIT_getMode(PowerLimit* me){
    return me->plMode;
}

sbyte2 POWERLIMIT_getTorqueCommand(PowerLimit* me){
    return me->plTorqueCommand;
}

ubyte1 POWERLIMIT_getTargetPower(PowerLimit* me){
    return me->plTargetPower;
}

ubyte1 POWERLIMIT_getInitialisationThreshold(PowerLimit* me){
    return me->plInitializationThreshold;
}

