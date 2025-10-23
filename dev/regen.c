#include "IO_Driver.h" //Includes datatypes, constants, etc - should be included in every c file
#include "motorController.h"
#include "regen.h"
#include "bms.h"


#define MIN_REGEN_SPEED_KPH 5
#define REGEN_RAMPDOWN_START_SPEED 10

Regen* Regen_new(bool regenToggle)
{
    Regen* me = (Regen*)malloc(sizeof(Regen));
    me->regenToggle=regenToggle;

    me->mode = REGENMODE_OFF;
    me->appsTorque = 0;
    me->bpsTorque = 0;
    me->regenTorqueCommand = 0;
    me->torqueLimitDNm = 0;
    me->torqueAtZeroPedalDNm = 0;
    me->percentBPSForMaxRegen = 0; //zero to one.. 1 = 100%
    me->percentAPPSForCoasting = 0;
    me->minimumSpeedKPH = MIN_REGEN_SPEED_KPH;       //Assigned by main
    me->SpeedRampStart = REGEN_RAMPDOWN_START_SPEED; //Assigned by main
    me->tick = 0;
    
    Regen_updateMode(me);

    return me;
}

void Regen_calculateCommands(Regen* me, MotorController * mcm, TorqueEncoder *tps, BrakePressureSensor *bps)
{
    if (me->regenToggle == FALSE) { 
        MCM_set_Regen_activeStatus(mcm, FALSE);
        return; 
    }
    MCM_set_Regen_activeStatus(mcm, TRUE);   

    if (MCM_getGroundSpeedKPH(mcm) < MIN_REGEN_SPEED_KPH) {     // no regen under 5kph rule
        MCM_set_Regen_activeStatus(mcm, FALSE);        
        return;
    }

    if (MCM_getDCCurrent(mcm) < -72) {      // saftey check stopping at -72A over 1 consecutive second
        me->tick++;
    } else {
        me->tick = 0;
    }

    if (me->tick >= 10) {
        MCM_set_Regen_activeStatus(mcm, FALSE);
        return;
    }
    
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

    // Shinika's APPS implementation: 
    me->appsTorque = MCM_getMaxTorqueDNm(mcm) * getPercent(appsOutputPercent, me->percentAPPSForCoasting, 1, TRUE) 
                    - me->torqueAtZeroPedalDNm * getPercent(appsOutputPercent, me->percentAPPSForCoasting, 0, TRUE);

    // Shinika's BPS implementation: 
    // me->bpsTorque  = 0 - (me->torqueLimitDNm - me->torqueAtZeroPedalDNm) * getPercent(bps->percent, 0, me->percentBPSForMaxRegen, TRUE);

    // Prop Valve Regen Implementation:
    me->bpsTorque = 1/2.7f ;//* (bps->Sensor_BPS0-bps->Sensor_BPS1 / 145.038f) * 0.5f * 791.73f * (148.34f / 1000.0f);
    //               ^                ^                 ^          ^       ^          ^          ^
    //               |                |                 |          |       |          |          |
    // gear ratio ----                |                 |          |       |          |          |
    // prop valve pressure ------------                 |          |       |          |          |
    // (PSI -> mPa) -------------------------------------          |       |          |          |
    // pad Mu ------------------------------------------------------       |          |          |
    // rear piston area (in mm^2) ------------------------------------------          |          |
    // rotor diameter (in mm) ---------------------------------------------------------          |
    // (mm -> m) ---------------------------------------------------------------------------------

    me->regenTorqueCommand = me->appsTorque + me->bpsTorque;

    MCM_set_Regen_torqueCommand(mcm, me->regenTorqueCommand);
}

void Regen_updateMode(Regen* me)
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

/*
ALL THE SHIT WE NEED GETTERS FOR
- PropValveSensorIn
- PropValveSensorOut
ig would be chill:
- pad mu
*/

sbyte2 Regen_getMode(Regen *me)
{
    return me->mode;
}
