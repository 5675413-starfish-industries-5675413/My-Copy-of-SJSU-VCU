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
    me->torquePID = PID_new(10, 0, 0, 231,10); // torque PID tuning
    me->powerPID = PID_new(5, 0, 0, 80,10);    // power PID tuning
    me->plMode = 2; // 1 = Torque PID, 2 = Power PID, 3 = Torque+Power PID
    me->plStatus = FALSE; // FALSE = Off, TRUE = On
    me->plTorqueCommand = 0; // Torque command in deciNewton-meters
    me->plTargetPower = 60;// HERE IS WHERE YOU CHANGE POWERLIMIT (units = kW)
    me->plThresholdDiscrepancy = 5; // Threshold discrepancy in kW
    me->plInitializationThreshold = 0; // Initialization threshold in kW
    me->clampingMethod = 4; // Clamping method
    me->plAlwaysOn = TRUE; // TRUE = if is above threshold then stay on even if power is below threshold, FALSE = Only on if power is above threshold
    
    // tracking variables for the tq+power switching
    me->previousPower = 0.0f;
    me->powerChangeRate = 0.0f;
    me->settlingCounter = 0;
    me->usePowerPID = FALSE;
    me->SETTLING_THRESHOLD = 10; // number of vcu cycles to confirm the settling
    me->POWER_CHANGE_THRESHOLD = 0.5f; // kW change rate threshold
    me->POWER_BELOW_SETPOINT_THRESHOLD = 2.0f; // kW below setpoint threshold
    
    return me;
}

void PowerLimit_setPLInitializationThreshold(PowerLimit* me){
    me->plInitializationThreshold = me->plTargetPower - me->plThresholdDiscrepancy;
}

void PowerLimit_entryConditions(PowerLimit* me, MotorController *mcm ){
     sbyte4 current_power_kw = (sbyte4) ((MCM_getDCVoltage(mcm) * MCM_getDCCurrent(mcm)) / 1000);
     if(MCM_commands_getAppsTorque(mcm) == 0 || current_power_kw <= 0){ // if no torque command or power is 0, turn off PL
            me->plStatus = FALSE;
            PowerLimit_PIDReset(me);
        }

    else{
        if (me->plAlwaysOn == FALSE)
        { // if PL is not always on, only turn on if power is above threshold and off if below
            if (current_power_kw > me->plInitializationThreshold) {
                me->plStatus = TRUE;
            } else {
                me->plStatus = FALSE;
                PowerLimit_PIDReset(me);
            }
        } else 
        { // if PL is always on, only turn on if power is above threshold and never turn off
            if (me->plStatus == FALSE && current_power_kw > me->plInitializationThreshold) {
                me->plStatus = TRUE;
                PowerLimit_PIDReset(me); // start PL with a clean slate
            }
        }
    }
}

void PowerLimit_PIDReset(PowerLimit* me){
    // Reset torque PID
    me->torquePID->proportional = 0;
    me->torquePID->integral = 0;
    me->torquePID->previousError = 0;
    me->torquePID->totalError = 0;
    me->torquePID->derivative = 0;
    
    // Reset power PID
    me->powerPID->proportional = 0;
    me->powerPID->integral = 0;
    me->powerPID->previousError = 0;
    me->powerPID->totalError = 0;
    me->powerPID->derivative = 0;
}


/** COMPUTATIONS **/
void PowerLimit_updatePLPower(PowerLimit* me){
    PLMode plModeFromRotary = getPLMode();
            switch (plModeFromRotary){
                case PL_MODE_OFF:
                    me->plToggle = FALSE; // Default to 40kW when off match the struct value
                    break;
                case PL_MODE_20:
                    me->plTargetPower = 20;
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
                default:
                    break;
                    
            }
}

void PowerLimit_calculateCommands(PowerLimit *me, MotorController *mcm, TorqueEncoder *tps){
    if(me->plToggle){
        //PowerLimit_updatePLPower(me); // uncomment for rotary switch
        PowerLimit_setPLInitializationThreshold(me);
        PowerLimit_entryConditions(me, mcm);
        

        if (me->plStatus){
            if(me->plMode==1){
                POWERLIMIT_TorquePID(me, mcm, FALSE);
            }
            if(me->plMode==2){
                POWERLIMIT_PowerPID(me,mcm, FALSE);
            }
            if(me->plMode==3){
                POWERLIMIT_TorquePID_PowerPID(me,mcm);
            }
            }
       
    }

    MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand);
    MCM_set_PL_updateStatus(mcm, me->plStatus);
 
}

void POWERLIMIT_TorquePID(PowerLimit *me, MotorController *mcm, bool preserveMode){
    if (!preserveMode) {
        me->plMode = 1;
        me->usePowerPID = FALSE;
    }
    PID_setSaturationPoint(me->torquePID, 231);

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
    POWERLIMIT_updatePIDController(me, torqueSetpointFloat, commandedTorque, TRUE); // TRUE = use torque PID
    float pidOutput = PID_getOutput(me->torquePID);
    me->plTorqueCommand = (sbyte2)((pidOutput + commandedTorque));


     MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand);
     MCM_set_PL_updateStatus(mcm, me->plStatus);
}

void POWERLIMIT_PowerPID(PowerLimit *me, MotorController *mcm, bool preserveMode){
    if (!preserveMode) {
        me->usePowerPID = TRUE;
        me->plMode = 2;
    }

    me->powerPID->saturationValue = 80;

    sbyte2 commandedTQ = (sbyte2)(MCM_getCommandedTorque(mcm));
    sbyte4 motorRPM = (MCM_getMotorRPM(mcm));
    sbyte4 dcVoltage = (MCM_getDCVoltage(mcm));
    sbyte4 dcCurrent = (MCM_getDCCurrent(mcm));
    

    float setpointPower = ((float)(me->plTargetPower));
    float drawnPower = (float)((dcVoltage*dcCurrent)/1000.0f);

    me->clampingMethod=4;
    POWERLIMIT_updatePIDController(me, setpointPower, drawnPower, FALSE);

    float pidOutput = PID_getOutput(me->powerPID);

    me->plTorqueCommand =(sbyte2)((pidOutput + drawnPower)* (9549.0f)/((float)motorRPM));    

    
    MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand); ///// set max to be based off of TPS reading and skip middle man, commanded TQ from MCM
    MCM_set_PL_updateStatus(mcm, me->plStatus);
}

void POWERLIMIT_PowerPID2(PowerLimit *me, MotorController *mcm){
    me->plMode = 2;
    me->usePowerPID = TRUE;
    me->powerPID->saturationValue = 80;

    sbyte4 motorRPM = (MCM_getMotorRPM(mcm));
    sbyte4 dcVoltage = (MCM_getDCVoltage(mcm));
    sbyte4 dcCurrent = (MCM_getDCCurrent(mcm));
    sbyte2 commandedTQ = ((dcVoltage * dcCurrent) * 9.549) / motorRPM;

    float setpointPower = ((float)(me->plTargetPower));
    float drawnPower = (float)((dcVoltage*dcCurrent)/1000.0f);

    me->clampingMethod=4;
    POWERLIMIT_updatePIDController(me, setpointPower, drawnPower, FALSE);

    float pidOutput = PID_getOutput(me->powerPID);

    me->plTorqueCommand =(sbyte2)((pidOutput + drawnPower)* (9549.0f)/((float)motorRPM));    

    
    MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand); ///// set max to be based off of TPS reading and skip middle man, commanded TQ from MCM
    MCM_set_PL_updateStatus(mcm, me->plStatus);
}

void POWERLIMIT_TorquePID_PowerPID(PowerLimit *me, MotorController *mcm){
    me->plMode = 3;
    PID_setSaturationPoint(me->torquePID, 231);
    
    // get power with current and voltage
    sbyte4 dcVoltage = MCM_getDCVoltage(mcm);
    sbyte4 dcCurrent = MCM_getDCCurrent(mcm);
    float drawnPower = (float)(dcVoltage * dcCurrent) / 1000.0f; // Convert to kW
    float powerSetpoint = (float)(me->plTargetPower);
    
    // calculate power change rate (kW per cycle)
    if (me->previousPower > 0.0f) {
        me->powerChangeRate = drawnPower - me->previousPower;
    }
    
    // check if we should switch to power PID
    if (!me->usePowerPID) {
        // Conditions to switch to power PID:
        // 1. drawn power is below setpoint by threshold
        // 2. Power change rate is small (settling)
        // 3. Power change rate is positive (approaching setpoint)
        if (drawnPower < (powerSetpoint - me->POWER_BELOW_SETPOINT_THRESHOLD) &&
            fabs(me->powerChangeRate) < me->POWER_CHANGE_THRESHOLD &&
            me->powerChangeRate > 0.0f) {
            me->settlingCounter++;
        } else {
            me->settlingCounter = 0; // reset counter if conditions not met
        }
        
        // switch to power PID if settling conditions met for enough cycles
        if (me->settlingCounter >= me->SETTLING_THRESHOLD) {
            me->usePowerPID = TRUE;
            me->settlingCounter = 0; // reset for potential switch back
        }
    } else {
        // check if we should switch back to torque PID
        // Conditions to switch back:
        // 1. Power is above setpoint (overshoot)
        // 2. Power change rate is large (unstable)
        if (drawnPower > powerSetpoint ||
            fabs(me->powerChangeRate) > (me->POWER_CHANGE_THRESHOLD * 2.0f)) {
            me->usePowerPID = FALSE;
            me->settlingCounter = 0;
        }
    }
    
    // execute appropriate PID method
    if (me->usePowerPID) {
        // use power PID for control when settling
        POWERLIMIT_PowerPID(me, mcm, TRUE);
    } else {
        // use torque PID for the initial start and reaching setpoint
        POWERLIMIT_TorquePID(me, mcm, TRUE);
    }
    
    // update previous power for next cycle
    me->previousPower = drawnPower;
    MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand);
    MCM_set_PL_updateStatus(mcm, me->plStatus);
}

void POWERLIMIT_updatePIDController(PowerLimit* me, float pidSetpoint, float sensorValue, bool useTorquePID) {
        PID* currentPID = POWERLIMIT_getCurrentPID(me, useTorquePID);
        float currentError = pidSetpoint - sensorValue;
        switch (me->clampingMethod) {
            case 0: //No clamping
                currentPID->antiWindupFlag = FALSE;
                break;
            case 1: 
                if (currentError > 0) {
                    currentPID->antiWindupFlag = TRUE;
                    PID_setSaturationPoint(currentPID, 231); 
                }
                else {
                    currentPID->antiWindupFlag = FALSE;
                }
                break;
            case 2: 
                if (currentError > 0) {
                    currentPID->antiWindupFlag = TRUE;
                    PID_setSaturationPoint(currentPID, sensorValue); 
                }
                else {
                    currentPID->antiWindupFlag = FALSE;
                }
                break;
            case 3: 
                if (currentError > 0) {
                    currentPID->antiWindupFlag = TRUE;
                    currentPID->totalError -= PID_getPreviousError(currentPID); 
                }
                else {
                    currentPID->antiWindupFlag = FALSE;
                }
                break;
            case 4: 
                if (currentError < 0) {
                    currentPID->antiWindupFlag = TRUE;
                    currentPID->totalError -= PID_getPreviousError(currentPID); 

                }
                else {
                    currentPID->antiWindupFlag = FALSE;
                }
                break;
            case 5: {
                float tentativeOutput = currentPID->proportional + currentPID->integral + currentPID->derivative + sensorValue;
                if (tentativeOutput > currentPID->saturationValue) {
                    currentPID->antiWindupFlag = TRUE;
                    float correction = currentPID->saturationValue - tentativeOutput;
                    float scaledCorrection = correction * currentPID->Ki;
                    currentPID->integral += scaledCorrection;
                }
                else {
                    currentPID->antiWindupFlag = FALSE;
                }
                break;
            }
            case 6: {
                if (currentPID->proportional + currentPID->integral + currentPID->derivative + sensorValue > (float)(currentPID->saturationValue)) {
                    currentPID->totalError -= PID_getPreviousError(currentPID);
                    currentPID->antiWindupFlag = TRUE;
                }
                else {
                    currentPID->antiWindupFlag = FALSE;
                }
                break;
            }
        }
        PID_updateSetpoint(currentPID, pidSetpoint);
        PID_computeOutput(currentPID, sensorValue);
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


PID* POWERLIMIT_getCurrentPID(PowerLimit* me, bool useTorquePID){
    return useTorquePID ? me->torquePID : me->powerPID;
}

bool POWERLIMIT_getUseTorquePID(PowerLimit* me){
    return !me->usePowerPID;
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

