/*****************************************************************************
 * powerLimit.c - Power Limiting using a PID controller to simplify calculations
 * Previous Author(s): Harleen Sandhu & Shaun Gilmore 
 * Current Author(s): Akash Karthik & Aitrieus Wright
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
#include "sensors.h"

#ifndef POWERLIMITCONSTANTS
#define POWERLIMITCONSTANTS

#endif

// #define ELIMINATE_CAN_MESSAGES
/** PARAMETERS **/
PowerLimit* POWERLIMIT_new(bool plToggle){
    PowerLimit* me = (PowerLimit*)malloc(sizeof(PowerLimit));
    me->plToggle=plToggle;
    me->pid = PID_new(10, 0, 0, 231,10); // last value tells you the factor the PID gets divided by
    me->plMode = 2; // 1 = Torque PID, 2 = Power PID
    me->plStatus = FALSE; // FALSE = Off, TRUE = On
    me->plTorqueCommand = 0; // Torque command in deciNewton-meters
    me->plTargetPower = 40;// HERE IS WHERE YOU CHANGE POWERLIMIT (units = kW)
    me->plThresholdDiscrepancy = 5; // Threshold discrepancy in kW
    me->plInitializationThreshold = 0; // Initialization threshold in kW
    me->clampingMethod = 4; // Clamping method
    me->plAlwaysOn = TRUE; // TRUE = if is above threshold then stay on even if power is below threshold, FALSE = Only on if power is above threshold
    return me;
}

void PowerLimit_setPLInitializationThreshold(PowerLimit* me){
    me->plInitializationThreshold = me->plTargetPower - me->plThresholdDiscrepancy;
}

void PowerLimit_entryConditions(PowerLimit* me, MotorController *mcm ){
     sbyte4 current_power_kw = (sbyte4) ((MCM_getDCVoltage(mcm) * MCM_getDCCurrent(mcm)) / 1000);
     if(MCM_commands_getAppsTorque(mcm) == 0|| current_power_kw < me->plInitializationThreshold){
            me->plStatus == FALSE;
        }

     if (current_power_kw <= 0){
            me->plStatus = FALSE;
        }
    else{
        if (me->plAlwaysOn==FALSE)
        { // if PL is not always on, only turn on if power is above threshold and off if below
            if (current_power_kw > me->plInitializationThreshold) {
                me->plStatus = TRUE;
            } else {
                me->plStatus = FALSE;
            }
        } else 
        { // if PL is always on, only turn on if power is above threshold and never turn off
            if (me->plStatus==FALSE && current_power_kw > me->plInitializationThreshold) {
                me->plStatus = TRUE;
            }
        }
    }
}
/** COMPUTATIONS **/
void PowerLimit_updatePLPower(PowerLimit* me){
    PLMode plModeFromRotary = getPLMode();
            switch (plModeFromRotary){
                case PL_MODE_OFF:
                    break;
                case PL_MODE_30:
                    me->plTargetPower = 30;
                    break;
                case PL_MODE_40:
                    me->plTargetPower = 40;
                    break;
                case PL_MODE_50:
                    me->plTargetPower = 50;
                    break;
                case PL_MODE_60:
                    me->plTargetPower = 60;
                    break;
                case PL_MODE_80:
                    me->plTargetPower = 80;
                    break;
                default:
                    break;
                    
            }
}

void PowerLimit_calculateCommands(PowerLimit *me, MotorController *mcm, TorqueEncoder *tps){

    if(me->plToggle){
        PowerLimit_updatePLPower(me);
        PowerLimit_setPLInitializationThreshold(me);
        PowerLimit_entryConditions(me, mcm);
        

        if (me->plStatus){
            if(me->plMode==1){
                POWERLIMIT_TorquePID(me, mcm);
            }
            if(me->plMode==2){
                POWERLIMIT_PowerPID(me,mcm);
            }
            if(me->plMode==3){
                POWERLIMIT_TorquePID_PowerPID(me,mcm);
            }
            }
       
    }

    MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand);
    MCM_set_PL_updateStatus(mcm, me->plStatus);
 
}

void POWERLIMIT_TorquePID(PowerLimit *me, MotorController *mcm){
    me->plMode = 1;
    PID_setSaturationPoint(me->pid, 231);

    sbyte4 motorRPM = MCM_getMotorRPM(mcm);
    if (motorRPM == 0){
        motorRPM = 1; //avoid division by 0
    }
    float commandedTorque = (float)MCM_getCommandedTorque(mcm);
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
    me->plTorqueCommand = (sbyte2)((pidOutput + commandedTorque));


     MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand);
     MCM_set_PL_updateStatus(mcm, me->plStatus);
}

void POWERLIMIT_PowerPID(PowerLimit *me, MotorController *mcm){
    me->plMode = 2;
    me->pid->saturationValue = 80;

    sbyte2 commandedTQ = (sbyte2)(MCM_getCommandedTorque(mcm));
    sbyte4 motorRPM = (MCM_getMotorRPM(mcm));
    sbyte4 dcVoltage = (MCM_getDCVoltage(mcm));
    sbyte4 dcCurrent = (MCM_getDCCurrent(mcm));
    

    float setpointPower = ((float)(me->plTargetPower));
    float drawnPower = (float)((dcVoltage*dcCurrent)/1000.0f);

    me->clampingMethod=4;
    POWERLIMIT_updatePIDController(me, setpointPower, drawnPower);

    float pidOutput = me->pid->output;

    me->plTorqueCommand =(sbyte2)((pidOutput + drawnPower)* (9549.0f)/((float)motorRPM));    

    
    MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand); ///// set max to be based off of TPS reading and skip middle man, commanded TQ from MCM
    MCM_set_PL_updateStatus(mcm, me->plStatus);
}

void POWERLIMIT_PowerPID2(PowerLimit *me, MotorController *mcm){
    me->plMode = 2;
    me->pid->saturationValue = 80;

    sbyte4 motorRPM = (MCM_getMotorRPM(mcm));
    sbyte4 dcVoltage = (MCM_getDCVoltage(mcm));
    sbyte4 dcCurrent = (MCM_getDCCurrent(mcm));
    sbyte2 commandedTQ = ((dcVoltage * dcCurrent) * 9.549) / motorRPM;

    float setpointPower = ((float)(me->plTargetPower));
    float drawnPower = (float)((dcVoltage*dcCurrent)/1000.0f);

    me->clampingMethod=4;
    POWERLIMIT_updatePIDController(me, setpointPower, drawnPower);

    float pidOutput = me->pid->output;

    me->plTorqueCommand =(sbyte2)((pidOutput + drawnPower)* (9549.0f)/((float)motorRPM));    

    
    MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand); ///// set max to be based off of TPS reading and skip middle man, commanded TQ from MCM
    MCM_set_PL_updateStatus(mcm, me->plStatus);
}

void POWERLIMIT_TorquePID_PowerPID(PowerLimit *me, MotorController *mcm){
    me->plMode = 3;
    PID_setSaturationPoint(me->pid, 231);
    
    // get power with current and voltage
    sbyte4 dcVoltage = MCM_getDCVoltage(mcm);
    sbyte4 dcCurrent = MCM_getDCCurrent(mcm);
    float drawnPower = (float)(dcVoltage * dcCurrent) / 1000.0f; // Convert to kW
    float powerSetpoint = (float)(me->plTargetPower);
    
    // variables to track power settling behavior
    float previousPower = 0.0f;
    float powerChangeRate = 0.0f;
    ubyte2 settlingCounter = 0;
    bool usePowerPID = FALSE;
    ubyte2 SETTLING_THRESHOLD = 10; // number of vcu cycles to confirm the settling
    float POWER_CHANGE_THRESHOLD = 0.5f; // kW change rate threshold
    float POWER_BELOW_SETPOINT_THRESHOLD = 2.0f; // kW below setpoint threshold
    
    // calculate power change rate (kW per cycle)
    if (previousPower > 0.0f) {
        powerChangeRate = drawnPower - previousPower;
    }
    
    // check if we should switch to power PID
    if (!usePowerPID) {
        // Conditions to switch to power PID:
        // 1. drawn power is below setpoint by threshold
        // 2. Power change rate is small (settling)
        // 3. Power change rate is positive (approaching setpoint)
        if (drawnPower < (powerSetpoint - POWER_BELOW_SETPOINT_THRESHOLD) &&
            fabs(powerChangeRate) < POWER_CHANGE_THRESHOLD &&
            powerChangeRate > 0.0f) {
            settlingCounter++;
        } else {
            settlingCounter = 0; // reset counter if conditions not met
        }
        
        // switch to power PID if settling conditions met for enough cycles
        if (settlingCounter >= SETTLING_THRESHOLD) {
            usePowerPID = TRUE;
            settlingCounter = 0; // reset for potential switch back
        }
    } else {
        // check if we should switch back to torque PID
        // Conditions to switch back:
        // 1. Power is above setpoint (overshoot)
        // 2. Power change rate is large (unstable)
        if (drawnPower > powerSetpoint ||
            fabs(powerChangeRate) > (POWER_CHANGE_THRESHOLD * 2.0f)) {
            usePowerPID = FALSE;
            settlingCounter = 0;
        }
    }
    
    // execute appropriate PID method
    if (usePowerPID) {
        // use power PID for control when settling
        POWERLIMIT_PowerPID(me, mcm);
    } else {
        // use torque PID for the initial start and reaching setpoint
        POWERLIMIT_TorquePID(me, mcm);
    }
    
    // update previous power for next cycle
    previousPower = drawnPower;
    MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand);
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
                if (currentError > 0) {
                    me->pid->antiWindupFlag = TRUE;
                    me->pid->totalError -= PID_getPreviousError(me->pid); 
                }
                else {
                    me->pid->antiWindupFlag = FALSE;
                }
                break;
            case 4: 
                if (currentError < 0) {
                    me->pid->antiWindupFlag = TRUE;
                    me->pid->totalError -= PID_getPreviousError(me->pid); 

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
                if (me->pid->proportional + me->pid->integral + me->pid->derivative + sensorValue > (float)(me->pid->saturationValue)) {
                    me->pid->totalError -= PID_getPreviousError(me->pid);
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

