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
#include "gps.h"
#include "sensors.h"
#include "shunt.h"
#include <stdlib.h>

// constants
#define TOTAL_ENERGY_BUDGET_KWH 6.6f // total energy budget for the entire race
#define CYCLE_TIME_S 0.01f           // 10ms vcu cycle time
#define SECONDS_PER_HOUR 3600.0f

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
    
    //lap start
    me->latitude = 0.0f;
    me->longitude = 0.0f;

    // current position
    me->currLatitude = 0.0f;
    me->currLongitude = 0.0f;

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

void Efficiency_calculateCommands(Efficiency *me, MotorController *mcm, PowerLimit *pl, GPS *gps, Shunt *shunt)
{
    if (!me->efficiencyToggle)
    {
        return;
    }

    if (gpsButtonPressed()) {
        Efficiency_markLapStart(me, gps);
    }

    me->currLatitude = GPS_getLatitude(gps);
    me->currLongitude = GPS_getLongitude(gps);

    // calculate current power consumption in kWh
    float currentPower_kW = ((float)shunt->current_mA * (float)shunt->vBus_mV) / 1e9f; // convert mA*mV to kW
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

    if (Efficiency_completedLap(me, mcm, gps))
    {
        // calculate the carryover for just one lap
        me->carryOverEnergy_kWh += me->energyBudget_kWh - me->lapEnergy_kWh;

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

void Efficiency_markLapStart(Efficiency *me, GPS *gps)
{
    if (!GPS_isValid(gps)) return;
    me->latitude = GPS_getLatitude(gps);
    me->longitude = GPS_getLongitude(gps);
}

bool Efficiency_completedLap(Efficiency *me, MotorController *mcm, GPS *gps)
{
    float wheelSpeed_kph = MCM_getGroundSpeedKPH(mcm);
    me->lapDistance_km += wheelSpeed_kph * CYCLE_TIME_S / SECONDS_PER_HOUR;

    if (me->lapDistance_km > 0.5f &&
        GPS_isValid(gps) &&
        (me->latitude != 0.0f && me->longitude != 0.0f) &&
        fabsf(GPS_getLatitude(gps) - me->latitude) <= 0.00005f &&
        fabsf(GPS_getLongitude(gps) - me->longitude) <= 0.00005f)
    {
        me->lapCounter++;
        me->finishedLap = TRUE;
    }
    else
    {
        me->finishedLap = FALSE;
    }
    return me->finishedLap;
}

// Score error scaled x10000 (e.g. 0.06 error → 600). Conversion done here.
sbyte2 Efficiency_getScoreError(Efficiency* me) {
    if (me->energyBudget_kWh < 0.001f) return 0;

    float lapFraction = me->lapDistance_km; // 0.0 → 1.0 km
    float expectedEnergy = lapFraction * me->energyBudget_kWh;
    float error = (me->lapEnergy_kWh - expectedEnergy) / me->energyBudget_kWh;
    return (sbyte2)(error * 10000.0f);
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

// Energy getters: internal storage is kWh, getter converts to Wh for CAN
sbyte2 Efficiency_getEnergyBudget_Wh(Efficiency *me) { return (sbyte2)(me->energyBudget_kWh * 1000.0f); }
sbyte2 Efficiency_getCarryOverEnergy_Wh(Efficiency *me) { return (sbyte2)(me->carryOverEnergy_kWh * 1000.0f); }
sbyte2 Efficiency_getCornerEnergy_Wh(Efficiency *me) { return (sbyte2)(me->cornerEnergy_kWh * 1000.0f); }
sbyte2 Efficiency_getStraightEnergy_Wh(Efficiency *me) { return (sbyte2)(me->straightEnergy_kWh * 1000.0f); }
sbyte2 Efficiency_getLapEnergy_Wh(Efficiency *me) { return (sbyte2)(me->lapEnergy_kWh * 1000.0f); }

// Time getter: truncates to whole seconds
sbyte2 Efficiency_getStraightTime_s(Efficiency *me) { return (sbyte2)(me->straightTime_s); }

// Distance getter: converts km → m
sbyte2 Efficiency_getLapDistance_m(Efficiency *me) { return (sbyte2)(me->lapDistance_km * 1000.0f); }

// Lat/lon getters: scaled x1000000 for integer transmission
sbyte4 Efficiency_getLapLatitude(Efficiency *me) { return (sbyte4)(me->latitude * 1000000.0f); }
sbyte4 Efficiency_getLapLongitude(Efficiency *me) { return (sbyte4)(me->longitude * 1000000.0f); }
sbyte4 Efficiency_getCurrentLatitude(Efficiency *me) { return (sbyte4)(me->currLatitude * 1000000.0f); }
sbyte4 Efficiency_getCurrentLongitude(Efficiency *me) { return (sbyte4)(me->currLongitude * 1000000.0f); }
