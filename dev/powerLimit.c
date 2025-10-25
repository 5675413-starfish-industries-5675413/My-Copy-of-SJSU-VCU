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
    me->pid = PID_new(10, 20, 0, 231,10); // PID tuning
    me->plMode = 2; // 1 = Torque PID, 2 = Power PID, 3 = Torque+Power PID
    me->plStatus = FALSE; // FALSE = Off, TRUE = On
    me->plTorqueCommand = 0; // Torque command in deciNewton-meters
    me->plTargetPower = 40;// HERE IS WHERE YOU CHANGE POWERLIMIT (units = kW)
    me->plThresholdDiscrepancy = 5; // Threshold discrepancy in kW
    me->plInitializationThreshold = 0; // Initialization threshold in kW
    me->clampingMethod = 4; // Clamping methods 0 (none), 4 (power), 6 (tq)
    me->plAlwaysOn = TRUE; // TRUE = if is above threshold then stay on even if power is below threshold, FALSE = Only on if power is above threshold
    
    return me;
}

void PowerLimit_setPLInitializationThreshold(PowerLimit* me){
    me->plInitializationThreshold = me->plTargetPower - me->plThresholdDiscrepancy;
}

void PowerLimit_PIDReset(PowerLimit* me){
    // Reset the PID
    me->pid->proportional = 0;
    me->pid->integral = 0;
    me->pid->previousError = 0;
    me->pid->totalError = 0;
    me->pid->derivative = 0;
}

void PowerLimit_entryConditions(PowerLimit* me, MotorController *mcm ){
     sbyte4 current_power_kw = (sbyte4) ((MCM_getDCVoltage(mcm) * MCM_getDCCurrent(mcm)) / 1000);
     if(MCM_commands_getAppsTorque(mcm) == 0 || current_power_kw <= 0){ // if no torque command or power is 0, turn off PL
            me->plStatus = FALSE;
            PowerLimit_PIDReset(me);
        }

    else
    {
        if (me->plAlwaysOn == FALSE)
        { // if PL is not always on, only turn on if power is above threshold and off if below
            if (current_power_kw > me->plInitializationThreshold) {
                me->plStatus = TRUE;
            } else {
                me->plStatus = FALSE;
                PowerLimit_PIDReset(me);
            }
        }
        else 
        { // if PL is always on, only turn on if power is above threshold and never turn off
            if (me->plStatus == FALSE && current_power_kw > me->plInitializationThreshold) {
                me->plStatus = TRUE;
            }
        }
    }
}


/** COMPUTATIONS **/
void PowerLimit_updatePLPower(PowerLimit* me){
    PLMode plModeFromRotary = getPLMode();
    ubyte1 previousTargetPower = me->plTargetPower;
            switch (plModeFromRotary){
                case PL_MODE_OFF:
                    me->plToggle = FALSE; // Default to 40kW when off match the struct value
                    break;
                case PL_MODE_20:
                    if(previousTargetPower != 30)
                        {PowerLimit_PIDReset(me);}
                    me->plTargetPower = 30;
                    me->plToggle = TRUE;
                    break;
                case PL_MODE_30:
                    if(previousTargetPower != 40)
                        {PowerLimit_PIDReset(me);}
                    me->plTargetPower = 40;
                    me->plToggle = TRUE;
                    break;
                case PL_MODE_40:
                    if(previousTargetPower != 50)
                        {PowerLimit_PIDReset(me);}
                    me->plTargetPower = 50;
                    me->plToggle = TRUE;
                    break;
                case PL_MODE_50:
                    if(previousTargetPower != 60)
                        {PowerLimit_PIDReset(me);}
                    me->plTargetPower = 60;
                    me->plToggle = TRUE;
                    break;
                case PL_MODE_60:
                    if(previousTargetPower != 70)
                        {PowerLimit_PIDReset(me);}
                    me->plTargetPower = 70;
                    me->plToggle = TRUE;
                    break;
                default:
                    break;
                    
            }
}

void PowerLimit_calculateCommands(PowerLimit *me, MotorController *mcm, TorqueEncoder *tps){
    PowerLimit_updatePLPower(me); // uncomment for rotary switch
    if(me->plToggle){
        //PowerLimit_updatePLPower(me); // uncomment for rotary switch
        PowerLimit_setPLInitializationThreshold(me);
        PowerLimit_entryConditions(me, mcm);

        if (me->plStatus){
            if(me->plMode==1){
                POWERLIMIT_TorquePID(me, mcm);
            }
            if(me->plMode==2){
                POWERLIMIT_PowerPID(me,mcm);
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

    POWERLIMIT_updatePIDController(me, torqueSetpointFloat, commandedTorque);
    float pidOutput = PID_getOutput(me->pid);
    me->plTorqueCommand = (sbyte2)((pidOutput + commandedTorque));


     MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand);
     MCM_set_PL_updateStatus(mcm, me->plStatus);
}

void POWERLIMIT_PowerPID(PowerLimit *me, MotorController *mcm){
    me->plMode = 2;
    me->pid->saturationValue = 80;

    sbyte4 motorRPM = (MCM_getMotorRPM(mcm));
    sbyte4 dcVoltage = (MCM_getDCVoltage(mcm));
    sbyte4 dcCurrent = (MCM_getDCCurrent(mcm));
    

    float setpointPower = ((float)(me->plTargetPower));
    float drawnPower = (float)((dcVoltage*dcCurrent)/1000.0f);

    POWERLIMIT_updatePIDController(me, setpointPower, drawnPower);
    float pidOutput = PID_getOutput(me->pid);

    me->plTorqueCommand =(sbyte2)((pidOutput + drawnPower)* (9549.0f)/((float)motorRPM));    

    
    MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand); ///// set max to be based off of TPS reading and skip middle man, commanded TQ from MCM
    MCM_set_PL_updateStatus(mcm, me->plStatus);
}

void POWERLIMIT_PowerPID2(PowerLimit *me, MotorController *mcm){
    me->plMode = 2;
    me->usePowerPID = TRUE;
    me->pid->saturationValue = 80;

    sbyte4 motorRPM = (MCM_getMotorRPM(mcm));
    sbyte4 dcVoltage = (MCM_getDCVoltage(mcm));
    sbyte4 dcCurrent = (MCM_getDCCurrent(mcm));
    sbyte2 commandedTQ = ((dcVoltage * dcCurrent) * 9.549) / motorRPM;

    float setpointPower = ((float)(me->plTargetPower));
    float drawnPower = (float)((dcVoltage*dcCurrent)/1000.0f);

    me->clampingMethod=4;
    POWERLIMIT_updatePIDController(me, setpointPower, drawnPower);

    float pidOutput = PID_getOutput(me->pid);

    me->plTorqueCommand =(sbyte2)((pidOutput + drawnPower)* (9549.0f)/((float)motorRPM));    

    
    MCM_update_PL_setTorqueCommand(mcm, me->plTorqueCommand); ///// set max to be based off of TPS reading and skip middle man, commanded TQ from MCM
    MCM_set_PL_updateStatus(mcm, me->plStatus);
}

void POWERLIMIT_updatePIDController(PowerLimit* me, float pidSetpoint, float sensorValue) {
    float currentError = pidSetpoint - sensorValue;
    switch (me->clampingMethod) {
        case 0: //No clamping
            me->pid->antiWindupFlag = FALSE;
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

