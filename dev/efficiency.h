#ifndef _EFFICIENCY_H
#define _EFFICIENCY_H

/*****************************************************************************
 * efficiency.h - Energy Budget Algorithm Header
 * Current Author(s): Akash Karthik
 *****************************************************************************/

#include "IO_Driver.h" //Includes datatypes, constants, etc - should be included in every c file
#include "motorController.h"
#include "powerLimit.h"
#include "gps.h"
#include "shunt.h"
#include <stdlib.h>

typedef struct _Efficiency
{
    bool efficiencyToggle;

    // Energy Budget Algorithm Variables (persistent across laps)
    float energyBudget_kWh;    // Energy budget per lap
    float carryOverEnergy_kWh; // Carry-over energy from previous lap
    ubyte1 lapCounter;         // Current lap counter

    // Lap Variables (reset at end of each lap)
    float straightTime_s;     // Time spent in straights this lap
    float cornerTime_s;       // Time spent in corners this lap
    float cornerEnergy_kWh;   // Energy spent in corners this lap
    float straightEnergy_kWh; // Energy spent in straights this lap
    float lapEnergy_kWh;      // Total energy used this lap
    float lapDistance_km;     // Accumulated distance this lap
    bool finishedLap;         // Flag indicating lap completion

    float latitude;           // Latitude at start of lap
    float longitude;          // Longitude at start of lap

    float currLatitude;       // Current Latitude
    float currLongitude;      // Current Longitude
} Efficiency;

// Constructor
Efficiency *Efficiency_new(bool efficiencyToggle);

// Main computation functions
void Efficiency_calculateCommands(Efficiency *me, MotorController *mcm, PowerLimit *pl, GPS *gps, Shunt *shunt);
void Efficiency_markLapStart(Efficiency *me, GPS *gps);
void Efficiency_resetLap(Efficiency *me);
bool Efficiency_completedLap(Efficiency *me, MotorController *mcm, GPS *gps);

// Getter functions
float Efficiency_getScoreError(Efficiency *me);
bool Efficiency_getFinishedLap(Efficiency *me);
ubyte1 Efficiency_getLapCounter(Efficiency *me);
float Efficiency_getEnergyBudget_kWh(Efficiency *me);
float Efficiency_getCarryOverEnergy_kWh(Efficiency *me);
float Efficiency_getCornerEnergy_kWh(Efficiency *me);
float Efficiency_getStraightEnergy_kWh(Efficiency *me);
float Efficiency_getLapEnergy_kWh(Efficiency *me);
sbyte4 Efficiency_getLapLatitude(Efficiency *me);
sbyte4 Efficiency_getLapLongitude(Efficiency *me);
float Efficiency_getStraightTime_s(Efficiency *me);
float Efficiency_getLapDistance_km(Efficiency *me);
float Efficiency_getCurrentLatitude(Efficiency *me);
float Efficiency_getCurrentLongitude(Efficiency *me);

#endif // _EFFICIENCY_H
