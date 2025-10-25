#include <stdlib.h> //Needed for malloc
#include "IO_Driver.h" //Includes datatypes, constants, etc - should be included in every c file
#include "motorController.h"
#include "sensors.h"
#include "regen.h"
#include "mathFunctions.h"

// Rules & Saftey Features
#define MIN_REGEN_SPEED_KPH                5
#define REGEN_RAMPDOWN_START_SPEED        10

// Pressure Formula Constants
#define GEAR_RATIO                      2.7f
#define PSI_TO_MPa                 0.006895f
#define mm_TO_m                       0.001f
#define REAR_PISTON_AREA         4 * 791.73f    // mm^2  //this is nolan this number should be 4x
#define ROTOR_RADIUS                  74.17f    // mm

Regen* Regen_new(bool regenToggle)
{
    Regen* me = (Regen*)malloc(sizeof(Regen));
    me->regenToggle=regenToggle;

    me->mode = REGENMODE_FORMULAE;
    me->appsTorque = 0;
    me->bpsTorqueNm = 0;
    me->regenTorqueCommand = 0;
    me->torqueLimitDNm = 0;
    me->torqueAtZeroPedalDNm = 0;
    me->percentBPSForMaxRegen = 0; //zero to one.. 1 = 100%
    me->percentAPPSForCoasting = 0;
    me->padMu = 0.4;
    me->minimumSpeedKPH = MIN_REGEN_SPEED_KPH;       //Assigned by main
    me->SpeedRampStart = REGEN_RAMPDOWN_START_SPEED; //Assigned by main
    me->tick = 0;

    Regen_updateSettings(me);

    return me;
}

void Regen_calculateCommands(Regen *me, MotorController *mcm, TorqueEncoder *tps, BrakePressureSensor *bps)
{
     BrakePressureSensor_setPSI(bps);
    if (me->regenToggle == FALSE) {
        MCM_set_Regen_activeStatus(mcm, FALSE);
        return;
    }

    if (MCM_getGroundSpeedKPH(mcm) < MIN_REGEN_SPEED_KPH) {
        MCM_set_Regen_activeStatus(mcm, FALSE);
        return;
    }

    if (MCM_getDCCurrent(mcm) < -72) { // saftey check stopping at -72A over 1 consecutive second
        me->tick++;
    } else {
        me->tick = 0;
    }

    if (me->tick >= 10) {
        MCM_set_Regen_activeStatus(mcm, FALSE);
        return;
    }

    MCM_set_Regen_activeStatus(mcm, TRUE);

/*
    // Regen mode is now set based on battery voltage to preserve overvoltage fault
    // if(BMS_getPackVoltage(bms) >= 38500 * 10){
    //     MCM_setRegenMode(mcm0, REGENMODE_FORMULAE);
    // } else {
    //     MCM_setRegenMode(mcm0, REGENMODE_FIXED);
    // }
*/

    float4 appsOutputPercent;
    TorqueEncoder_getOutputPercent(tps, &appsOutputPercent);

    me->torqueLimitDNm = 750;
    if (MCM_getMotorRPM(mcm) > 2400){
        me->torqueLimitDNm = -0.022 * (MCM_getMotorRPM(mcm)-2400) + 750; //No regen above 2400 RPM
    }
    
    me->appsTorque = MCM_getMaxTorqueDNm(mcm) * appsOutputPercent;

    if (me->mode == REGENMODE_HYBRID) {
        // Shinika's APPS implementation:
        me->appsTorque = MCM_getMaxTorqueDNm(mcm) * getPercent(appsOutputPercent, me->percentAPPSForCoasting, 1, TRUE)
                       - me->torqueAtZeroPedalDNm * getPercent(appsOutputPercent, me->percentAPPSForCoasting, 0, TRUE);
    } 
    else if (me->mode == REGENMODE_FORMULAE &&  me->appsTorque > 0) {
        MCM_set_Regen_torqueCommand(mcm, me->appsTorque); // redundancy: no bps if throttle is pressed (might happen due to noise in sensors)
    }

    // Shinika's BPS implementation:
    // me->bpsTorqueDnm = 0 - (me->torqueLimitDNm - me->torqueAtZeroPedalDNm) * getPercent(bps->percent, 0, me->percentBPSForMaxRegen, TRUE);

    // Prop Valve Regen Implementation:
   
    me->bpsTorqueNm = (((bps->bps0_PSI-bps->bps1_PSI) * PSI_TO_MPa) * me->padMu * REAR_PISTON_AREA * (ROTOR_RADIUS * mm_TO_m)) / GEAR_RATIO;

    me->regenTorqueCommand = (me->appsTorque/10) - (sbyte2)(me->bpsTorqueNm);

    if (me->regenTorqueCommand >= 231) {
        MCM_set_Regen_torqueCommand(mcm, 231);
    }
    else if (me->regenTorqueCommand <= -50) {
        MCM_set_Regen_torqueCommand(mcm, 50);
    }
    else {
        MCM_set_Regen_torqueCommand(mcm, me->regenTorqueCommand);
    }
}

void Regen_updateSettings(Regen* me)
{
    switch (me->mode)
    {
    case REGENMODE_FORMULAE:    // Regen only on brakes
        me->torqueLimitDNm = 500;//me->torqueMaximumDNm * 0.5;
        me->torqueAtZeroPedalDNm = 0;
        me->percentAPPSForCoasting = 0;
        me->percentBPSForMaxRegen = .3; //zero to one.. 1 = 100%
        break;

    case REGENMODE_TESLA:       // Regen only on throttle
        me->torqueLimitDNm = 500;//me->torqueMaximumDNm * .5;
        me->torqueAtZeroPedalDNm = me->torqueLimitDNm;
        me->percentAPPSForCoasting = .1;
        me->percentBPSForMaxRegen = 0;
        break;

    case REGENMODE_HYBRID:      // Mix of FormulaE & Tesla (light "engine braking")
        me->torqueLimitDNm = 500;//me->torqueMaximumDNm * 0.5;
        me->torqueAtZeroPedalDNm = me->torqueLimitDNm * 0.3;
        me->percentAPPSForCoasting = .2;
        me->percentBPSForMaxRegen = .3; //zero to one.. 1 = 100%
        break;

    case REGENMODE_FIXED:       // Fixed regen (outdated)
        me->torqueLimitDNm = 250; //1000 = 100Nm
        me->torqueAtZeroPedalDNm = me->torqueLimitDNm;
        me->percentAPPSForCoasting = .05;
        me->percentBPSForMaxRegen = 0;
        break;

    // TODO:  User customizable regen settings - Issue #97
    case REGENMODE_CUSTOM:
        me->torqueLimitDNm = 0;
        me->torqueAtZeroPedalDNm = 0;
        me->percentBPSForMaxRegen = 0; //zero to one.. 1 = 100%
        me->percentAPPSForCoasting = 0;
        break;

    case REGENMODE_OFF:
        me->torqueLimitDNm = 0;
        me->torqueAtZeroPedalDNm = 0;
        me->percentBPSForMaxRegen = 0; //zero to one.. 1 = 100%
        me->percentAPPSForCoasting = 0;
        break;
    }
}

sbyte2 Regen_get_torqueCommand(Regen *me) {
    return me->regenTorqueCommand;
}
