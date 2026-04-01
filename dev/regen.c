#include <stdlib.h> //Needed for malloc
#include "IO_Driver.h" //Includes datatypes, constants, etc - should be included in every c file
#include "motorController.h"
#include "sensors.h"
#include "regen.h"
#include "mathFunctions.h"

// Rules & Saftey Features
#define REGEN_MIN_SPEED_KPH                5    // Car cannot regen under 5km/hr per rules
#define REGEN_CURRENT_LIMIT              -72    // Overcurrent threshold (will shut down if exceeds for too long)
#define REGEN_OVERCURRENT_TICK_LIMIT      10    // How many consecutive ticks past overcurrent threshold before shutdown
#define REGEN_RAMPDOWN_START_SPEED        10

// Imperical Conversions
#define kPa_TO_MPa                    0.001f
#define mm_TO_m                       0.001f

// SR-17 Constants (used for prop valve Pressure <--> Torque conversion)
#define GEAR_RATIO                     2.92f    // (12:35 Enduro) (10:35 Accel)
#define REAR_PISTON_AREA         4 * 791.73f    // mm^2 | 4x, because 4 calipers total (2 for each wheel)
#define ROTOR_RADIUS                 163.32f    // mm   | effective rear rotor radius

Regen* Regen_new(bool regenToggle)
{
    Regen* me = (Regen*)malloc(sizeof(Regen));
    
    me->regenToggle = regenToggle;          // on/off switch
    me->appsTorque = 0;                     // regen on throttle (if none, simply linear map of APPS)
    me->bpsTorque = 0;                      // regen on brakes
    me->regenTorqueCommand = 0;             // when negative, mcm regens
    me->mode = REGENMODE_FORMULAE;
    me->regenOnThrottle = FALSE;
    me->regenOnBrakes = REGEN_BPS_LINEAR;
    me->regenTorqueLimit = -50;             // Nm | MCM regens when torque is negative

    // regen-on-APPS
    me->maxRegenAPPS = 0;                   // torque sent at zero throttle
    me->percentAPPSForCoasting = 0;         // APPS% required to coast w/ regen-on-APPS enabled

    // regen-on-brakes
    me->padMu = 0.45;                       // treated as constant. TODO: might be good idea to relate w/ rotor temps
    me->percentBPSForMaxRegen  = 0;         // what % of BPS activates max regen-on-brakes (legacy)

    // saftey check(s)
    me->tick = 0;

    Regen_updateSettings(me);

    return me;
}

void Regen_updateSettings(Regen* me)
{
    switch (me->mode)
    {
    // for parameters starting with percent...      range is 0-1 (.50 = 50%; 1 = 100%, etc.)
    
    case REGENMODE_TESLA:       // regen-on-throttle ONLY
        me-> regenOnThrottle = TRUE;
        me-> regenOnBrakes = REGEN_BPS_OFF;             // regen-on-brakes is disabled

        // Tunable parameters:
        // me->regenTorqueLimit = -50;
        me->maxRegenAPPS = me->regenTorqueLimit;        // all of regen-tq-limit allocated for regen-on-throttle
        me->percentAPPSForCoasting = .10;               // what % of throttle for driver to coast
        break;

    case REGENMODE_FORMULAE:    // regen-on-brakes ONLY. doesn't account for Brake Bias (legacy)
        me-> regenOnThrottle = FALSE;
        me-> regenOnBrakes = REGEN_BPS_LINEAR;          // linear mapping of brakes using BPS (legacy)
        me->maxRegenAPPS = 0;                           // no regenTQ allocated to regen-on-throttle
        me->percentAPPSForCoasting = 0;                 // driver should coast when off throttle

        // Tunable parameters:
        // me->regenTorqueLimit = -50;
        me->percentBPSForMaxRegen = .30;                // what % of brakes to command full regen
        break;

    case REGENMODE_HYBRID_LEGACY:   // TESLA + FormulaE: regen on throttle AND brakes (via linear mapping)
        me-> regenOnThrottle = TRUE;
        me-> regenOnBrakes = REGEN_BPS_LINEAR;          // linear mapping of brakes using BPS (legacy)

        // Tunable parameters:
        // me->regenTorqueLimit = -50;
        me->maxRegenAPPS = me->regenTorqueLimit * 0.3;  // how much regen-on-throttle is sent at zero pedal
        me->percentAPPSForCoasting = .20;               // what % of throttle for driver to coast
        me->percentBPSForMaxRegen = .30;                // what % of brakes to command full regen
        break;

    case REGENMODE_INVIS:           // prop valve regen ONLY. accounts for Brake Bias
        me-> regenOnThrottle = FALSE;
        me-> regenOnBrakes = REGEN_BPS_INVIS;           // prop valve pressure removed --> regen torque

        // Tunable parameters:
        // me->regenTorqueLimit = -50;
        me->maxRegenAPPS = 0;                           // 0% allocated to regen-on-throttle
        me->padMu = 0.45;                               // used in pressure --> torque conversion. treated as a constant

    case REGENMODE_HYBRID_MODERN:   // TESLA + INVIS: regen on throttle AND brakes (via prop valve)
        me-> regenOnThrottle = TRUE;
        me-> regenOnBrakes = REGEN_BPS_INVIS;           // prop valve pressure removed --> regen torque

        // Tunable parameters:
        // me->regenTorqueLimit = -50;
        me->maxRegenAPPS = me->regenTorqueLimit * 0.3;  // how much regen-on-throttle is sent at zero pedal
        me->percentAPPSForCoasting = .20;               // what % of throttle for driver to coast
        me->padMu = 0.45;                               // used in pressure --> torque conversion. treated as a constant
        break;

    case REGENMODE_OFF:
        me-> regenOnThrottle = FALSE;
        me-> regenOnBrakes = REGEN_BPS_OFF;
        me->regenTorqueLimit = 0;
        me->maxRegenAPPS = 0;
        break;
    }
}

void Regen_calculateCommands(Regen *me, MotorController *mcm, TorqueEncoder *tps, BrakePressureSensor *bps)
{
    // Always Read Sensors
    BrakePressureSensor_setPressure(bps);

    //Exit if Regen is disabled
    if (!me->regenToggle)
	{
        me->mode = REGENMODE_OFF;
        MCM_set_Regen_activeStatus(mcm, FALSE);
        return;
    }

    // Early returns
    Regen_updateState(me, mcm, tps, bps);


    Regen_calculateAPPSTorque(me, mcm, tps);
    Regen_calculateBPSTorque(me, mcm, bps);

    // Can't brake and accelerate at the same time, per rules (TODO: find this rule & cite on DDR)
    if (bps->bps0_percent > 0) { // TODO: probably should move this
        me->appsTorque = 0; // bps overpowers tps
    }

    me->regenTorqueCommand = (sbyte2)(me->appsTorque - me->bpsTorque);

    Regen_clamp(me, mcm);
}

void Regen_updateState(Regen *me, MotorController *mcm, TorqueEncoder *tps, BrakePressureSensor *bps)
{
    // Disables regen under 5km/hr, per rules
    if (MCM_getGroundSpeedKPH(mcm) < REGEN_MIN_SPEED_KPH && me->regenTorqueCommand < 0) {
        me->mode = REGENMODE_OFF;
        MCM_set_Regen_activeStatus(mcm, FALSE);
        return;
    }

    // Saftey check: too much regen current for 1 consecutive second
    if (MCM_getDCCurrent(mcm) < REGEN_CURRENT_LIMIT) {
        me->tick++;
    } else {
        me->tick = 0;
    }
    if (me->tick >= REGEN_OVERCURRENT_TICK_LIMIT) {
        me->mode = REGENMODE_OFF;
        MCM_set_Regen_activeStatus(mcm, FALSE);
        me->regenToggle = FALSE; // permanantly disables regen
        return;
    }

/*  This is reportedly commented our b/c it doesn't work. This should be updated, fixed, & implemented
    // Saftey Check: Early return based on battery voltage to prevent overvoltage fault
    if(BMS_getPackVoltage(bms) >= 38500 * 10){
        me->mode = REGENMODE_OFF;
        MCM_set_Regen_activeStatus(mcm, FALSE);
    }
*/

/*  We can get rid of this saftey check: Source Aidan @ Regen DDR
    if (MCM_getMotorRPM(mcm) > 2400){
        me->regenTorqueLimit -= 0.22 * (MCM_getMotorRPM(mcm)-2400); //Torque limit is reduced by -0.022 DNm for each RPM over 2400
    }
*/

    MCM_set_Regen_activeStatus(mcm, TRUE);
}

void Regen_calculateAPPSTorque(Regen *me, MotorController *mcm, TorqueEncoder *tps)
{
    switch (me->regenOnThrottle) {
        float4 appsOutputPercent;
        TorqueEncoder_getOutputPercent(tps, &appsOutputPercent);

        case TRUE:  // Shinika's implementation (pedal map):
        me->appsTorque = MCM_getMaxTorqueDNm(mcm) * getPercent(appsOutputPercent, me->percentAPPSForCoasting, 1, TRUE)
                               - me->maxRegenAPPS * getPercent(appsOutputPercent, me->percentAPPSForCoasting, 0, TRUE);
        case FALSE: // Linear mapping of tps
            me->appsTorque = (MCM_getMaxTorqueDNm(mcm) / 10) * appsOutputPercent;
    }
}

void Regen_calculateBPSTorque(Regen *me, MotorController *mcm, TorqueEncoder *tps)
{
    switch (me->regenOnBrakes) {
        case REGEN_BPS_LINEAR: // Linear mapping: doesn't account for Brake Bias
            me->bpsTorque = 0 - (me->regenTorqueLimit - me->maxRegenAPPS) * getPercent(bps->percent, 0, me->percentBPSForMaxRegen, TRUE);
        case REGEN_BPS_INVIS: // Prop Valve implementation: (inlet - outlet pressure) --> torque, accounts for Brake Bias
            me->bpsTorque = 0 - ((bps->bps0_Pressure - bps->bps1_Pressure) * kPa_TO_MPa * me->padMu * REAR_PISTON_AREA * (ROTOR_RADIUS * mm_TO_m)) / GEAR_RATIO;
        case REGEN_BPS_OFF:
            me->bpsTorque = 0;
    }
}

void Regen_clamp(Regen *me, MotorController *mcm)
{
    // Clamps:
    if (me->regenTorqueCommand > 231) {
        MCM_set_Regen_torqueCommand(mcm, 231);  // clamps postive torque to 231 Nm
    }
    else if (me->regenTorqueCommand < (me->regenTorqueLimit)) {
        MCM_set_Regen_torqueCommand(mcm, me->regenTorqueLimit); // clamps regen to regen-tq-limit
    }
    else {
        MCM_set_Regen_torqueCommand(mcm, me->regenTorqueCommand);
    }
}

sbyte2 Regen_get_torqueCommand(Regen *me) {
    return me->regenTorqueCommand;
}
