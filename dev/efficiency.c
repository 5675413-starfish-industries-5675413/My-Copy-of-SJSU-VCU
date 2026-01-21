/*****************************************************************************
 * efficiency.c -
 * Current Author(s): Akash Karthik
 ******************************************************************************
 * Energy Budget Algorithm Implementation
 ****************************************************************************/
#include "IO_Driver.h" //Includes datatypes, constants, etc - should be included in every c file
#include "efficiency.h"
#include "powerLimit.h"
#include "motorController.h"
#include "IO_RTC.h"
#include "math.h"

// constants
#define TOTAL_ENERGY_BUDGET_KWH 5.5f // 80 kWh total energy budget
#define CYCLE_TIME_S 0.01f           // 10ms cycle time
#define SECONDS_PER_HOUR 3600.0f
#define WATTS_PER_KILOWATT 1000.0f

Efficiency *Efficiency_new(bool efficiencyToggle)
{
    Efficiency *me = (Efficiency *)malloc(sizeof(Efficiency));
    if (me == NULL)
    {
        return NULL;
    }

    me->efficiencyToggle = efficiencyToggle;

    // event variables
    me->energyBudget_kWh = TOTAL_ENERGY_BUDGET_KWH / 22; // set actual energy budget fpr each lap
    me->lapCounter = 1;
    me->carryOverEnergy_kWh = 0.0f; // start with no carry-over

    // lap variables (reset at the end of each lap)
    me->straightTime_s = 0.0f;
    me->cornerTime_s = 0.0f;
    me->cornerEnergy_kWh = 0.0f;
    me->straightEnergy_kWh = 0.0f;
    me->lapEnergy_kWh = 0.0f;
    me->lapDistance_km = 0.0f;
    me->finishedLap = FALSE;

    return me;
}

void Efficiency_calculateCommands(Efficiency *me, MotorController *mcm, PowerLimit *pl)
{
    if (!me->efficiencyToggle)
    {
        return;
    }

    // calculate current power consumption in kWh
    float currentPower_kW = (MCM_getDCVoltage(mcm) * MCM_getDCCurrent(mcm)) / WATTS_PER_KILOWATT;
    float cycleEnergy_kWh = (currentPower_kW * CYCLE_TIME_S) / SECONDS_PER_HOUR;

    // track time and accumulate energy based on power limit status
    if (pl->plStatus)
    {
        me->straightTime_s += CYCLE_TIME_S;
        me->straightEnergy_kWh += cycleEnergy_kWh;
    }
    else
    {
        me->cornerTime_s += CYCLE_TIME_S;
        me->cornerEnergy_kWh += cycleEnergy_kWh;
    }
    me->lapEnergy_kWh = me->straightEnergy_kWh + me->cornerEnergy_kWh;

    // Check if lap is complete
    Efficiency_completedLap(me, mcm);
    if (me->finishedLap)
    {
        // calculate the carryover for just one lap
        me->carryOverEnergy_kWh = me->energyBudget_kWh - me->lapEnergy_kWh;

        // calculate the power limit for next lap using the algorithm
        float straightTime_h = me->straightTime_s / SECONDS_PER_HOUR;
        if (straightTime_h > 0.0f)
        { // safety check division by zero
            pl->plTargetPower = ((me->energyBudget_kWh + me->carryOverEnergy_kWh) - me->cornerEnergy_kWh) / straightTime_h;
        }
        // reset baby!!
        Efficiency_resetLap(me);
    }
}

void Efficiency_completedLap(Efficiency *me, MotorController *mcm)
{
    // accumulate the distance traveled this cycle
    float wheelSpeed_kph = MCM_getGroundSpeedKPH(mcm); // later switch to wheelspeeds.c
    float distance_this_cycle_km = wheelSpeed_kph * CYCLE_TIME_S / SECONDS_PER_HOUR;
    me->lapDistance_km += distance_this_cycle_km;

    if (me->lapDistance_km > 1.0f)
    { // if accumulated distance is greater than 1 km, then we have completed a lap
        me->lapCounter++;
        me->finishedLap = TRUE;
    }
    else
    {
        me->finishedLap = FALSE;
    }
}

void Efficiency_resetLap(Efficiency *me)
{
    me->straightTime_s = 0.0f;
    me->cornerTime_s = 0.0f;
    me->cornerEnergy_kWh = 0.0f;
    me->straightEnergy_kWh = 0.0f;
    me->lapEnergy_kWh = 0.0f;
    me->lapDistance_km = 0.0f;
    me->finishedLap = FALSE;
}

// getters
bool Efficiency_getFinishedLap(Efficiency *me) { return me->finishedLap; }
ubyte1 Efficiency_getLapCounter(Efficiency *me) { return me->lapCounter; }
float Efficiency_getEnergyBudget_kWh(Efficiency *me) { return me->energyBudget_kWh; }
float Efficiency_getCarryOverEnergy_kWh(Efficiency *me) { return me->carryOverEnergy_kWh; }
float Efficiency_getCornerEnergy_kWh(Efficiency *me) { return me->cornerEnergy_kWh; }
float Efficiency_getStraightEnergy_kWh(Efficiency *me) { return me->straightEnergy_kWh; }
float Efficiency_getLapDistance_km(Efficiency *me) { return me->lapDistance_km; }
float Efficiency_getLapEnergy_kWh(Efficiency *me) { return me->lapEnergy_kWh; }
float Efficiency_getStraightTime_s(Efficiency *me) { return me->straightTime_s; }
