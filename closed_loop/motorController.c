#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "motorController.h"


// Mock function declarations (add these to your mock file)
extern void IO_RTC_StartTime(ubyte4* timestamp);
extern ubyte4 IO_RTC_GetTimeUS(ubyte4 start_timestamp);
extern void IO_DO_Set(ubyte1 pin, bool state);
extern void SerialManager_send(SerialManager* sm, const char* message);
extern void TorqueEncoder_getOutputPercent(TorqueEncoder* te, float4* outputPercent);
extern void RTDS_setVolume(ReadyToDriveSound* rtds, float4 volume, ubyte4 duration);
extern void Light_set(void* light, float4 brightness);

// Mock defines
#define IO_DO_00 0
#define TRUE 1
#define FALSE 0

// External sensor declarations (add these to your mock file)
extern Sensor Sensor_RTDButton;
extern Sensor Sensor_HVILTerminationSense;
extern void* Light_dashRTD;


struct _MotorController
{
    SerialManager *serialMan;
    
    // Controller statuses/properties
    ubyte2 canMessageBaseId;
    ubyte4 timeStamp_inverterEnabled;

    ubyte2 torqueMaximumDNm; // Max torque that can be commanded in deciNewton*meters

    //Regen torque calculations in whole Nm..?
    RegenMode regen_mode;                //Software reading of regen knob position.  Each mode has different regen behavior (variables below).
    ubyte2 regen_torqueLimitDNm;         //Tuneable value.  Regen torque (in Nm) at full regen.  Positive value.
    ubyte2 regen_torqueAtZeroPedalDNm;   //Tuneable value.  Amount of regen torque (in Nm) to apply when both pedals at 0% travel.  Positive value.
    float4 regen_percentBPSForMaxRegen;  //Tuneable value.  Amount of brake pedal required for full regen. Value between zero and one.
    float4 regen_percentAPPSForCoasting; //Tuneable value.  Amount of accel pedal required to exit regen.  Value between zero and one.
    sbyte1 regen_minimumSpeedKPH;        //Assigned by main
    sbyte1 regen_SpeedRampStart;

    bool relayState;
    bool previousHVILState;
    ubyte4 timeStamp_HVILLost;

    ubyte4 timeStamp_HVILOverrideCommandReceived;
    bool HVILOverride;

    ubyte4 timeStamp_InverterEnableOverrideCommandReceived;
    ubyte4 timeStamp_InverterDisableOverrideCommandReceived;
    Status InverterOverride;

    ubyte1 startupStage;
    Status lockoutStatus;
    Status inverterStatus;
    bool startRTDS;

    ubyte1 faultHistory[8];

    sbyte2 motor_temp;
    sbyte4 DC_Voltage;
    sbyte4 DC_Current;

    sbyte2 commandedTorque;
    ubyte4 currentPower;

    sbyte4 motorRPM;

    
    // Control parameters
    ubyte4 timeStamp_lastCommandSent;
    ubyte2 updateCount;

    sbyte2 commands_torque;
    sbyte2 appsTorque;
    sbyte2 bpsTorque;
    sbyte2 commands_torqueLimit;
    ubyte1 commands_direction;
    Status commands_discharge;
    Status commands_inverter;
};

MotorController *MotorController_new(SerialManager *sm, ubyte2 canMessageBaseID, Direction initialDirection, sbyte2 torqueMaxInDNm, sbyte1 minRegenSpeedKPH, sbyte1 regenRampdownStartSpeed)
{
    MotorController *me = (MotorController *)malloc(sizeof(struct _MotorController));
    if (!me) return NULL;
    
    me->serialMan = sm;
    me->canMessageBaseId = canMessageBaseID;
    
    // Reset timestamp
    MCM_commands_resetUpdateCountAndTime(me);

    me->lockoutStatus = UNKNOWN;
    me->inverterStatus = UNKNOWN;

    me->motorRPM = 100;
    me->DC_Voltage = 13;
    me->DC_Current = 13;

    me->commands_direction = initialDirection;
    me->commands_torqueLimit = me->torqueMaximumDNm = torqueMaxInDNm;

    me->regen_mode = REGENMODE_OFF;
    me->regen_torqueLimitDNm = 0;
    me->regen_torqueAtZeroPedalDNm = 0;
    me->regen_percentBPSForMaxRegen = 1; //zero to one.. 1 = 100%
    me->regen_percentAPPSForCoasting = 0;
    me->regen_minimumSpeedKPH = minRegenSpeedKPH;       //Assigned by main
    me->regen_SpeedRampStart = regenRampdownStartSpeed; //Assigned by main

    me->startupStage = 0; // Off
    me->relayState = FALSE;
    me->motor_temp = 99;
    me->HVILOverride = FALSE;

    return me;
}

void MCM_setRegenMode(MotorController *me, RegenMode regenMode)
{
    switch (regenMode)
    {
    case REGENMODE_FORMULAE: //Position 1 = Coasting mode (Formula E mode)
        me->regen_mode = 1;
        me->regen_torqueLimitDNm = me->torqueMaximumDNm * 0.5;
        me->regen_torqueAtZeroPedalDNm = 0;
        me->regen_percentAPPSForCoasting = 0;
        me->regen_percentBPSForMaxRegen = .3; //zero to one.. 1 = 100%
        break;

    case REGENMODE_HYBRID: //Position 2 = light "engine braking" (Hybrid mode)
        me->regen_mode = 2;
        me->regen_torqueLimitDNm = me->torqueMaximumDNm * 0.5;
        me->regen_torqueAtZeroPedalDNm = me->regen_torqueLimitDNm * 0.3;
        me->regen_percentAPPSForCoasting = .2;
        me->regen_percentBPSForMaxRegen = .3; //zero to one.. 1 = 100%
        break;

    case REGENMODE_TESLA: //Position 3 = One pedal driving (Tesla mode)
        me->regen_mode = 3;
        me->regen_torqueLimitDNm = me->torqueMaximumDNm * .5;
        me->regen_torqueAtZeroPedalDNm = me->regen_torqueLimitDNm;
        me->regen_percentAPPSForCoasting = .1;
        me->regen_percentBPSForMaxRegen = 0;
        break;

    case REGENMODE_FIXED: //Position 4 = Fixed Regen
        me->regen_mode = 4;
        me->regen_torqueLimitDNm = 250; //1000 = 100Nm
        me->regen_torqueAtZeroPedalDNm = me->regen_torqueLimitDNm;
        me->regen_percentAPPSForCoasting = .05;
        me->regen_percentBPSForMaxRegen = 0;
        break;

        // TODO:  User customizable regen settings - Issue #97
        // case REGENMONDE_CUSTOM:
        //     me->regen_mode = 4;
        //     me->regen_torqueLimitDNm = 0;
        //     me->regen_torqueAtZeroPedalDNm = 0;
        //     me->regen_percentBPSForMaxRegen = 0; //zero to one.. 1 = 100%
        //     me->regen_percentAPPSForCoasting = 0;
        //     break;

    case REGENMODE_OFF:
    default:
        me->regen_mode = REGENMODE_OFF;
        me->regen_torqueLimitDNm = 0;
        me->regen_torqueAtZeroPedalDNm = 0;
        me->regen_percentAPPSForCoasting = 0;
        me->regen_percentBPSForMaxRegen = 0; //zero to one.. 1 = 100%
        break;
    }
}

void MCM_calculateCommands(MotorController *me, TorqueEncoder *tps, BrakePressureSensor *bps)
{
    MCM_commands_setDischarge(me, DISABLED);
    MCM_commands_setDirection(me, FORWARD);
    me->appsTorque = 0;
    me->bpsTorque = 0;

    sbyte2 torqueOutput = 0;

    float4 appsOutputPercent;
    TorqueEncoder_getOutputPercent(tps, &appsOutputPercent);

    me->appsTorque = me->torqueMaximumDNm * appsOutputPercent;
    me->appsTorque = me->torqueMaximumDNm * getPercent(appsOutputPercent, me->regen_percentAPPSForCoasting, 1, TRUE) - me->regen_torqueAtZeroPedalDNm * getPercent(appsOutputPercent, me->regen_percentAPPSForCoasting, 0, TRUE);
    me->bpsTorque = 0 - (me->regen_torqueLimitDNm - me->regen_torqueAtZeroPedalDNm) * getPercent(bps->percent, 0, me->regen_percentBPSForMaxRegen, TRUE);

    /** MOTOR TORQUE COMMAND LOGIC **/
    // abstraction might be warranted for the below logic

    torqueOutput = me->appsTorque + me->bpsTorque;

    /////////////////////////////////////////////
    // LAUNCH CONTROL & POWER LIMITING         //
    /////////////////////////////////////////////

    // if(me->launchControlState == TRUE && me->lcTorqueCommand < me->appsTorque)
    // {
    //     torqueOutput = me->lcTorqueCommand;
    // } 
    // if(me->plActive == TRUE && me->plTorqueCommand < me->appsTorque)
    // {
    //     me->launchControlState == FALSE;
    //     torqueOutput = me->plTorqueCommand + bpsTorque;
    // }
    // //Safety Check. torqueOutput Should never rise above 231
    // if(torqueOutput > 2310 || torqueOutput < 0) //attempt to fix issue of -3000
    // {
    //     torqueOutput = me->appsTorque+bpsTorque;
    // }
    MCM_commands_setTorqueDNm(me, torqueOutput);

    //Causes MCM relay to be driven after 30 seconds with TTC60?
    // me->HVILOverride = (IO_RTC_GetTimeUS(me->timeStamp_HVILOverrideCommandReceived) < 1000000);

    //Temporarily disable MCM relay control via HVILOverride
    //me->HVILOverride = FALSE;

    // Inverter override logic
    if (IO_RTC_GetTimeUS(me->timeStamp_InverterDisableOverrideCommandReceived) < 1000000)
        me->InverterOverride = DISABLED;
    else if (IO_RTC_GetTimeUS(me->timeStamp_InverterEnableOverrideCommandReceived) < 1000000)
        me->InverterOverride = ENABLED;
    else
        me->InverterOverride = UNKNOWN;
}

void MCM_relayControl(MotorController *me, Sensor *HVILTermSense)
{
    // If HVIL Term Sense is low (HV is down)
    if (HVILTermSense->sensorValue == FALSE && me->HVILOverride == FALSE)
    {
        // If we just noticed the HVIL went low
        if (me->previousHVILState == TRUE)
        {
            SerialManager_send(me->serialMan, "Term sense went low\n");
            IO_RTC_StartTime(&me->timeStamp_HVILLost);
        }

        // If the MCM is on (and we lost HV)
        if (me->relayState == TRUE)
        {
            // Turn MCM off once 0 torque is commanded, or after 2 sec
            if (me->commandedTorque == 0 || IO_RTC_GetTimeUS(me->timeStamp_HVILLost) > 2000000)
            {
                IO_DO_Set(IO_DO_00, FALSE);
                me->relayState = FALSE;
            }
        }
        
        MCM_setStartupStage(me, 0);
        MCM_updateInverterStatus(me, UNKNOWN);
        MCM_updateLockoutStatus(me, UNKNOWN);

        me->previousHVILState = FALSE;
    }
    else // HVIL is high or override is active
    {
        // If HVIL just changed, send a message
        if (me->previousHVILState == FALSE)
        {
            SerialManager_send(me->serialMan, "Term sense went high\n");
            if (MCM_getStartupStage(me) == 0)
            {
                MCM_setStartupStage(me, 1);
            }
        }
        me->previousHVILState = TRUE;

        // Turn on the MCM relay
        IO_DO_Set(IO_DO_00, TRUE);
        me->relayState = TRUE;
    }
}

void MCM_inverterControl(MotorController *me, TorqueEncoder *tps, BrakePressureSensor *bps, ReadyToDriveSound *rtds)
{
    float4 RTDPercent = (Sensor_RTDButton.sensorValue == FALSE ? 1 : 0);
    
    switch (MCM_getStartupStage(me))
    {
    case 0: // MCM relay is off
    case 1: // MCM relay is on, lockout=enabled, inverter=disabled
        MCM_commands_setInverter(me, DISABLED);

        if (MCM_getLockoutStatus(me) == DISABLED)
        {
            SerialManager_send(me->serialMan, "MCM lockout has been disabled.\n");
            MCM_setStartupStage(me, 2);
        }
        break;

    case 2: // MCM on, lockout=disabled, inverter=disabled --> wait for RTD button
        if (Sensor_RTDButton.sensorValue == FALSE && 
            tps->calibrated == TRUE && 
            bps->calibrated == TRUE && 
            tps->travelPercent < .05 && 
            bps->percent > .25)
        {
            MCM_commands_setInverter(me, ENABLED);
            SerialManager_send(me->serialMan, "Changed MCM inverter command to ENABLE.\n");
            MCM_setStartupStage(me, 3);
        }
        break;

    case 3: // Waiting for inverter to be enabled
        if (MCM_getInverterStatus(me) == ENABLED)
        {
            RTDPercent = 1;
            SerialManager_send(me->serialMan, "Inverter enabled. Starting RTDS. Car ready to drive.\n");
            RTDS_setVolume(rtds, 1, 1500000);
            MCM_setStartupStage(me, 4);
        }
        break;

    case 4: // RTDS started
        SerialManager_send(me->serialMan, "RTD procedure complete.\n");
        RTDPercent = 1;
        MCM_setStartupStage(me, 5);
        break;

    case 5: // Ready to drive
        RTDPercent = 1;
        break;

    default:
        SerialManager_send(me->serialMan, "ERROR: Lost track of MCM startup status.\n");
        break;
    }

    // Inverter Command Overrides
    switch (me->InverterOverride)
    {
    case DISABLED:
        MCM_commands_setInverter(me, DISABLED);
        break;
    case ENABLED:
        MCM_commands_setInverter(me, ENABLED);
        break;
    case UNKNOWN:
        break;
    }

    // Set RTD light
    Light_set(Light_dashRTD, RTDPercent);
}

void MCM_parseCanMessage(MotorController *me, IO_CAN_DATA_FRAME *mcmCanMessage)
{
    static const ubyte1 bitInverter = 1;  // bit 1
    static const ubyte1 bitLockout = 128; // bit 7

    switch (mcmCanMessage->id)
    {
    case 0x0A2: // Temperature data
        me->motor_temp = ((ubyte2)mcmCanMessage->data[5] << 8 | mcmCanMessage->data[4]) / 10;
        break;

    case 0x0A5: // Motor speed
        me->motorRPM = (ubyte2)mcmCanMessage->data[3] << 8 | mcmCanMessage->data[2];
        break;

    case 0x0A6: // Current data
        me->DC_Current = ((ubyte2)mcmCanMessage->data[7] << 8 | mcmCanMessage->data[6]) / 10;
        break;

    case 0x0A7: // Voltage data
        me->DC_Voltage = ((ubyte2)mcmCanMessage->data[1] << 8 | mcmCanMessage->data[0]) / 10;
        break;

    case 0x0AA: // Status data
        me->inverterStatus = (mcmCanMessage->data[6] & bitInverter) > 0 ? ENABLED : DISABLED;
        me->lockoutStatus = (mcmCanMessage->data[6] & bitLockout) > 0 ? ENABLED : DISABLED;
        break;

    case 0x0AC: // Torque data
        me->commandedTorque = ((ubyte2)mcmCanMessage->data[1] << 8 | mcmCanMessage->data[0]) / 10;
        break;

    case 0x5FF: // Override commands
        if (mcmCanMessage->data[1] > 0)
        {
            IO_RTC_StartTime(&me->timeStamp_HVILOverrideCommandReceived);
        }
        if (mcmCanMessage->data[2] == 55)
        {
            IO_RTC_StartTime(&me->timeStamp_InverterEnableOverrideCommandReceived);
        }
        if (mcmCanMessage->data[3] > 0)
        {
            IO_RTC_StartTime(&me->timeStamp_InverterDisableOverrideCommandReceived);
        }
        break;
    }
}

// Command setters
void MCM_commands_setTorqueDNm(MotorController *me, sbyte2 newTorque)
{
    me->updateCount += (me->commands_torque == newTorque) ? 0 : 1;
    me->commands_torque = newTorque;
}

void MCM_commands_setDirection(MotorController *me, Direction newDirection)
{
    ubyte1 dir = (newDirection == FORWARD || newDirection == CLOCKWISE || newDirection == _0) ? 0 : 1;
    me->updateCount += (me->commands_direction == dir) ? 0 : 1;
    me->commands_direction = dir;
}

void MCM_commands_setInverter(MotorController *me, Status newInverterState)
{
    me->updateCount += (me->commands_inverter == newInverterState) ? 0 : 1;
    me->commands_inverter = newInverterState;
}

void MCM_commands_setDischarge(MotorController *me, Status setDischargeTo)
{
    me->updateCount += (me->commands_discharge == setDischargeTo) ? 0 : 1;
    me->commands_discharge = setDischargeTo;
}

void MCM_commands_setTorqueLimit(MotorController *me, sbyte2 newTorqueLimit)
{
    me->updateCount += (me->commands_torqueLimit == newTorqueLimit) ? 0 : 1;
    me->commands_torqueLimit = newTorqueLimit;
}

// Command getters
sbyte2 MCM_commands_getTorque(MotorController *me)
{
    return me->commands_torque;
}

Direction MCM_commands_getDirection(MotorController *me)
{
    return me->commands_direction;
}

Status MCM_commands_getInverter(MotorController *me)
{
    return me->commands_inverter;
}

Status MCM_commands_getDischarge(MotorController *me)
{
    return me->commands_discharge;
}

sbyte2 MCM_commands_getTorqueLimit(MotorController *me)
{
    return me->commands_torqueLimit;
}

// Status functions
void MCM_updateLockoutStatus(MotorController *me, Status newState)
{
    me->lockoutStatus = newState;
}

void MCM_updateInverterStatus(MotorController *me, Status newState)
{
    me->inverterStatus = newState;
}

Status MCM_getLockoutStatus(MotorController *me)
{
    return me->lockoutStatus;
}

Status MCM_getInverterStatus(MotorController *me)
{
    return me->inverterStatus;
}

bool MCM_getHvilOverrideStatus(MotorController *me)
{
    return me->HVILOverride;
}

Status MCM_getInverterOverrideStatus(MotorController *me)
{
    return me->InverterOverride;
}

void MCM_setRTDSFlag(MotorController *me, bool enableRTDS)
{
    me->startRTDS = enableRTDS;
}

bool MCM_getRTDSFlag(MotorController *me)
{
    return me->startRTDS;
}

// Timing functions
ubyte2 MCM_commands_getUpdateCount(MotorController *me)
{
    return me->updateCount;
}

void MCM_commands_resetUpdateCountAndTime(MotorController *me)
{
    me->updateCount = 0;
    IO_RTC_StartTime(&(me->timeStamp_lastCommandSent));
}

ubyte4 MCM_commands_getTimeSinceLastCommandSent(MotorController *me)
{
    return IO_RTC_GetTimeUS(me->timeStamp_lastCommandSent);
}

// Motor data getters
ubyte2 MCM_getTorqueMax(MotorController *me)
{
    return me->torqueMaximumDNm;
}

sbyte2 MCM_getAppsTorque(MotorController *me){
    return me->appsTorque;
}

sbyte2 MCM_getBpsTorque(MotorController *me){
    return me->bpsTorque;
}

sbyte4 MCM_getPower(MotorController *me)
{
    return ((me->DC_Voltage) * (me->DC_Current));
}

ubyte2 MCM_getCommandedTorque(MotorController *me)
{
    return me->commandedTorque;
}

sbyte2 MCM_getTemp(MotorController *me)
{
    return me->motor_temp;
}

sbyte2 MCM_getMotorTemp(MotorController *me)
{
    return me->motor_temp+me->commands_torque/50;
}

sbyte4 MCM_getGroundSpeedKPH(MotorController *me, sbyte4 motorRPM, sbyte4 FD_Ratio, sbyte4 Revolutions, sbyte4 tireCirc, sbyte4 KPH_Unit_Conversion)
{
    me->motorRPM = motorRPM;
    return ((me->motorRPM/FD_Ratio) * Revolutions * tireCirc) / KPH_Unit_Conversion; 
    // return me->groundSpeedKPH;
}

// Startup stage functions
void MCM_setStartupStage(MotorController *me, ubyte1 stage)
{
    me->startupStage = stage;
}

ubyte1 MCM_getStartupStage(MotorController *me)
{
    return me->startupStage;
}

// Torque limit functions
void MCM_setMaxTorqueDNm(MotorController* me, ubyte2 newTorque)
{
    me->torqueMaximumDNm = newTorque;
}

ubyte2 MCM_getMaxTorqueDNm(MotorController* me)
{
    return me->torqueMaximumDNm;
}

// Helper Functions
float4 getPercent(float4 input, float4 min, float4 max, bool increasing) {
    if (increasing) {
        if (input <= min) return 0;
        if (input >= max) return 1;
        return (input - min) / (max - min);
    } else {
        if (input <= min) return 1;
        if (input >= max) return 0;
        return 1 - ((input - min) / (max - min));
    }
}