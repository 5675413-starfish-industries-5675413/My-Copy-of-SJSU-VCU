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
    me->powerLimit = POWERLIMIT_new(efficiencyToggle);
    
    // algorithm variables
    me->energyBudget_kWh = ENERGY_BUDGET_PER_KM_KWH;  // set actual energy budget
    me->carryOverEnergy_kWh = 0.0f;  // start with no carry-over
    me->timeInStraights_s = 0.0f;
    me->timeInCorners_s = 0.0f;
    me->energySpentInCorners_kWh = 0.0f;
    me->energySpentInStraights_kWh = 0.0f;
    me->totalEnergyUsed_kWh = 0.0f;
    me->powerLimit_kW = 0.0f;
    return me;
}

void Efficiency_calculateCommands(Efficiency* me, MotorController *mcm) {
    if (!me->efficiencyToggle) {
        return;
    }
    
    // calculate current power consumption in kWh
    float currentPower_kW = (MCM_getDCVoltage(mcm) * MCM_getDCCurrent(mcm)) / 1000.0f;
    float energyThisCycle_kWh = (currentPower_kW * CYCLE_TIME_S) / 3600.0f;
    
    // track time and accumulate energy based on power limit status
    if (me->powerLimit->plStatus == TRUE) {
        me->timeInStraights_s += CYCLE_TIME_S;
        me->energySpentInStraights_kWh += energyThisCycle_kWh;
    } else {
        me->timeInCorners_s += CYCLE_TIME_S;
        me->energySpentInCorners_kWh += energyThisCycle_kWh;
    }
    
    me->totalEnergyUsed_kWh = me->energySpentInStraights_kWh + me->energySpentInCorners_kWh;
    
    // convert time from seconds to hours for the calculation
    float timeInStraights_h = me->timeInStraights_s / 3600.0f;
    
    if (timeInStraights_h > 0.0f) {
        // calculate the powerlimit using the algorithm
        me->powerLimit_kW = ((me->energyBudget_kWh + me->carryOverEnergy_kWh) - me->energySpentInCorners_kWh) / timeInStraights_h;

    } else {
        me->powerLimit_kW = 0.0f;
    }
    
}
