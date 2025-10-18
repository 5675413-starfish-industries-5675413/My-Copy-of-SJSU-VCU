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

// Constants
#define ENERGY_BUDGET_PER_KM_KWH 0.3f  // 0.3 kWh per km (from algorithm description)
#define CYCLE_TIME_S 0.1f  // 100ms cycle time

Efficiency* EFFICIENCY_new(bool efficiencyToggle){
    Efficiency* me = (Efficiency*)malloc(sizeof(Efficiency));
    me->efficiencyToggle = efficiencyToggle;
    
    // algorithm variables
    me->energyBudget_kWh = ENERGY_BUDGET_PER_KM_KWH;  // set actual energy budget
    me->carryOverEnergy_kWh = 0.0f;  // start with no carry-over
    me->timeInStraights_s = 0.0f;
    me->timeInCorners_s = 0.0f;
    me->energySpentInCorners_kWh = 0.0f;
    me->energySpentInStraights_kWh = 0.0f;
    me->totalEnergyUsed_kWh = 0.0f;
    me->totalLapDistance_km = 0.0f;
    me->finishedLap = FALSE;
    return me;
}

void Efficiency_calculateCommands(Efficiency* me, MotorController *mcm, PowerLimit *pl) {
    if (!me->efficiencyToggle) {
        return;
    }
    
    // calculate current power consumption in kWh
    float currentPower_kW = (MCM_getDCVoltage(mcm) * MCM_getDCCurrent(mcm)) / 1000.0f;
    float energyThisCycle_kWh = (currentPower_kW * CYCLE_TIME_S) / 3600.0f;
    
    // track time and accumulate energy based on power limit status
    if (pl->plStatus == TRUE && mcm->plTorqueCommand < mcm->appsTorque) {
        me->timeInStraights_s += CYCLE_TIME_S;
        me->energySpentInStraights_kWh += energyThisCycle_kWh;
    } else {
        me->timeInCorners_s += CYCLE_TIME_S;
        me->energySpentInCorners_kWh += energyThisCycle_kWh;
    }
    
    me->totalEnergyUsed_kWh = me->energySpentInStraights_kWh + me->energySpentInCorners_kWh;
    me->carryOverEnergy_kWh = me->energyBudget_kWh - me->totalEnergyUsed_kWh;
    // convert time from seconds to hours for the calculation
    float timeInStraights_h = me->timeInStraights_s / 3600.0f;
    Efficiency_completeLap(me, mcm);
    if (me->finishedLap) {
        // calculate the powerlimit using the algorithm
        pl->plTargetPower = ((me->energyBudget_kWh + me->carryOverEnergy_kWh) - me->energySpentInCorners_kWh) / timeInStraights_h;
        Efficiency_resetLap(me);
    }

}

void Efficiency_resetLap(Efficiency* me){
    me->timeInStraights_s = 0.0f;
    me->timeInCorners_s = 0.0f;
    me->energySpentInCorners_kWh = 0.0f;   
    me->energySpentInStraights_kWh = 0.0f;
    me->totalEnergyUsed_kWh = 0.0f;
    me->totalLapDistance_km = 0.0f;
    me->finishedLap = FALSE;
}

void Efficiency_completeLap(Efficiency* me, MotorController *mcm){
    // accumulate the distance traveled this cycle
    float wheelSpeed_kph = MCM_getGroundSpeedKPH(mcm);
    float distance_this_cycle_km = wheelSpeed_kph * CYCLE_TIME_S / 3600.0f;
    me->totalLapDistance_km += distance_this_cycle_km;
    if (me->totalLapDistance_km > 1.0f) { // if accumulated distance is greater than 1 km, then we have completed a lap
        me->finishedLap = TRUE;

    }
    else {
        me->finishedLap = FALSE;
    }
}