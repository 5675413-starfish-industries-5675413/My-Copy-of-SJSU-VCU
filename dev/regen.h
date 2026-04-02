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

// Regen mode & its derivative (regen on brakes)
typedef enum { REGENMODE_TESLA, REGENMODE_FORMULAE, REGENMODE_INVIS, REGENMODE_HYBRID_LEGACY, REGENMODE_HYBRID_MODERN, REGENMODE_OFF} RegenMode;
typedef enum { REGEN_BPS_OFF, REGEN_BPS_LINEAR, REGEN_BPS_INVIS } RegenOnBrakes;

// Define a structure for Regen
typedef struct _Regen {
    //Regen torque calculations in whole Nm..?
    bool regenToggle;
    RegenMode mode;                //Software reading of regen knob position.  Each mode has different regen behavior (variables below).
    bool regenOnThrottle;
    RegenOnBrakes regenOnBrakes;
    sbyte2 regenTorqueLimit;         //Tuneable value.  Regen torque (in Nm) at full regen.  Positive value.
    float4 appsTorque;
    float4 bpsTorque;
    sbyte2 regenTorqueCommand;
    ubyte2 maxRegenAPPS;   //Tuneable value.  Amount of regen torque (in Nm) to apply when both pedals at 0% travel.  Positive value.
    float4 percentBPSForMaxRegen;  //Tuneable value.  Amount of brake pedal required for full regen. Value between zero and one.
    float4 percentAPPSForCoasting; //Tuneable value.  Amount of accel pedal required to exit regen.  Value between zero and one.
    float4 padMu;
    sbyte1 tick;
} Regen;

/** CONSTRUCTOR **/
Regen* Regen_new(bool regenToggle); 

/** SETTER FUNCTIONS **/
void Regen_updateState(Regen *me, MotorController *mcm, TorqueEncoder *tps, BrakePressureSensor *bps);
void Regen_calculateCommands(Regen* me, MotorController * mcm, TorqueEncoder *tps, BrakePressureSensor *bps);
void Regen_calculateAPPSTorque(Regen *me, MotorController *mcm, TorqueEncoder *tps);
void Regen_calculateBPSTorque(Regen *me, MotorController *mcm, BrakePressureSensor *bps);
void Regen_updateMCMTorqueCommand(Regen *me, MotorController *mcm);
void Regen_updateSettings(Regen* me);

/** GETTER FUNCTIONS **/
sbyte2 Regen_get_torqueCommand(Regen *me);

#endif //_REGEN_H