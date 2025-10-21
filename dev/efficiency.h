#include "IO_Driver.h" //Includes datatypes, constants, etc - should be included in every c file
#include "motorController.h"
#include "PID.h"
#include "math.h"
#include "powerLimit.h"

typedef struct _Efficiency {
    bool efficiencyToggle;
    
    // Energy Budget Algorithm Variables
    float energyBudget_kWh;  // Energy Budget per lap (for now im doing 0.3 kWh)
    float carryOverEnergy_kWh;  // Carry Over Energy from previous lap
    float timeInStraights_s;  // Time in Straights
    float timeInCorners_s;  // Time in Corners
    float energySpentInCorners_kWh;  // Energy spent in corners
    float energySpentInStraights_kWh;  // Energy spent in straights
    float lapEnergySpent_kWh;  // Total energy used this lap
    float powerLimit_kW; // Power Limit calculated for next lap
    float totalLapDistance_km;  // Accumulated distance this lap
    bool finishedLap;  // Flag for lap completion
    PowerLimit *powerLimit;
    
} Efficiency;

Efficiency* EFFICIENCY_new(bool efficiencyToggle);
void Efficiency_calculateCommands(Efficiency* me, MotorController *mcm, PowerLimit *pl);
void Efficiency_resetLap(Efficiency* me);
void Efficiency_completedLap(Efficiency* me, MotorController *mcm);
