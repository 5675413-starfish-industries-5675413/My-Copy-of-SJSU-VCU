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

// event variables
#define TOTAL_ENERGY_BUDGET_KWH 5.5f  // 80 kWh total energy budget
#define CYCLE_TIME_S 0.01f  // 10ms cycle time

Efficiency* EFFICIENCY_new(bool efficiencyToggle){
    Efficiency* me = (Efficiency*)malloc(sizeof(Efficiency));
    me->efficiencyToggle = efficiencyToggle;
        
    // event variables
    me->energyBudget_kWh = TOTAL_ENERGY_BUDGET_KWH/22;  // set actual energy budget fpr each lap
    me->lapCounter = 1;

    me->carryOverEnergy_kWh = 0.0f;  // start with no carry-over

    // lap variables (reset at the end of each lap)    
    me->timeInStraights_s = 0.0f;
    me->timeInCorners_s = 0.0f;
    me->energySpentInCorners_kWh = 0.0f; 
    me->energySpentInStraights_kWh = 0.0f; 
    me->lapEnergySpent_kWh = 0.0f; 
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
    if (pl->plStatus == TRUE && MCM_get_PL_torqueCommand(mcm) < MCM_commands_getAppsTorque(mcm)) {
        me->timeInStraights_s += CYCLE_TIME_S;
        me->energySpentInStraights_kWh += energyThisCycle_kWh;
    } else {
        me->timeInCorners_s += CYCLE_TIME_S;
        me->energySpentInCorners_kWh += energyThisCycle_kWh;
    }
    
    me->lapEnergySpent_kWh = me->energySpentInStraights_kWh + me->energySpentInCorners_kWh;
    
    // Check if lap is complete
    Efficiency_completedLap(me, mcm);
    if (me->finishedLap) {
        // calculate the carryover for just one lap
        me->carryOverEnergy_kWh = me->energyBudget_kWh - me->lapEnergySpent_kWh;
        me->lapEnergySpent_kWh = 0.0f;
        
        // calculate the power limit for next lap using the algorithm
        float timeInStraights_h = me->timeInStraights_s / 3600.0f;  
        if (timeInStraights_h > 0.0f) { // safety check division by zero
            pl->plTargetPower = ((me->energyBudget_kWh + me->carryOverEnergy_kWh) - me->energySpentInCorners_kWh) / timeInStraights_h;
            me->energySpentInCorners_kWh = 0.0f;
            me->energySpentInStraights_kWh = 0.0f;
        }
        
        // reset el variablialas! for next lap
        Efficiency_resetLap(me);
    }
}

void Efficiency_completedLap(Efficiency* me, MotorController *mcm){
    // accumulate the distance traveled this cycle
    float wheelSpeed_kph = MCM_getGroundSpeedKPH(mcm); // later switch to wheelspeeds.c
    float distance_this_cycle_km = wheelSpeed_kph * CYCLE_TIME_S / 3600.0f;
    me->totalLapDistance_km += distance_this_cycle_km;
    if (me->totalLapDistance_km > 1.0f) { // if accumulated distance is greater than 1 km, then we have completed a lap
        me->lapCounter++;
        me->finishedLap = TRUE;

    }
    else {
        me->finishedLap = FALSE;
    }
}

void Efficiency_resetLap(Efficiency* me){
    me->timeInStraights_s = 0.0f;
    me->timeInCorners_s = 0.0f;
    me->energySpentInCorners_kWh = 0.0f;   
    me->energySpentInStraights_kWh = 0.0f;
    me->lapEnergySpent_kWh = 0.0f; //not reset
    me->totalLapDistance_km = 0.0f;
    me->finishedLap = FALSE;
}



//getters
bool Efficiency_getFinishedLap(Efficiency* me){
    return me->finishedLap;
}
ubyte1 Efficiency_getLapCounter(Efficiency* me){
    return me->lapCounter;
}


/*
pltargetpower should equal:
(getEnergyBudget_kWh + getCarryOverEnergy_kWh - getEnergySpentInCorners_kWh) / (getTimeInStraights_s/3600.0f)

*/
float Efficiency_getEnergyBudget_kWh(Efficiency* me){
    return me->energyBudget_kWh;
}
float Efficiency_getCarryOverEnergy_kWh(Efficiency* me){
    return me->carryOverEnergy_kWh;
}
float Efficiency_getEnergySpentInCorners_kWh(Efficiency* me){
    return me->energySpentInCorners_kWh;
}
float Efficiency_getTimeInStraights_s(Efficiency* me){
    return me->timeInStraights_s;
}
float Efficiency_getTotalLapDistance_km(Efficiency* me){
    return me->totalLapDistance_km;
}

float Efficiency_getLapEnergySpent_kWh(Efficiency* me){
    return me->lapEnergySpent_kWh;
}

