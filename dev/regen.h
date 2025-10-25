/*****************************************************************************
 * V1
 * Initial Author(s): Vern Toor
 ******************************************************************************
 * Regen using a Prop Valve implementation
 ****************************************************************************/
#ifndef _REGEN_H
#define _REGEN_H

#include "IO_Driver.h" //Includes datatypes, constants, etc - should be included in every c file
#include "motorController.h"
#include "sensors.h"
#include "mathFunctions.h"

// maybe also include sensors for prop valve

// Regen mode
typedef enum { REGENMODE_FORMULAE, REGENMODE_TESLA, REGENMODE_HYBRID, REGENMODE_FIXED, REGENMODE_CUSTOM, REGENMODE_OFF } RegenMode;

// Define a structure for Regen
typedef struct _Regen {
    //Regen torque calculations in whole Nm..?
    bool regenToggle;
    RegenMode mode;                //Software reading of regen knob position.  Each mode has different regen behavior (variables below).
    ubyte2 torqueLimitDNm;         //Tuneable value.  Regen torque (in Nm) at full regen.  Positive value.
    float4 appsTorque;
    float4 bpsTorqueNm;
    sbyte2 regenTorqueCommand;
    ubyte2 torqueAtZeroPedalDNm;   //Tuneable value.  Amount of regen torque (in Nm) to apply when both pedals at 0% travel.  Positive value.
    float4 percentBPSForMaxRegen;  //Tuneable value.  Amount of brake pedal required for full regen. Value between zero and one.
    float4 percentAPPSForCoasting; //Tuneable value.  Amount of accel pedal required to exit regen.  Value between zero and one.
    sbyte1 minimumSpeedKPH;        //Assigned by main
    sbyte1 SpeedRampStart;
    float4 padMu;
    sbyte1 tick;
} Regen;

/** CONSTRUCTOR **/
Regen* Regen_new(bool regenToggle); 

/** SETTER FUNCTIONS **/
void Regen_calculateCommands(Regen* me, MotorController * mcm, TorqueEncoder *tps, BrakePressureSensor *bps);
void Regen_updateMode(Regen* me);

/** GETTER FUNCTIONS **/
sbyte2 Regen_get_torqueCommand(Regen *me);

#endif //_REGEN_H
