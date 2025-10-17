#include <stdint.h>
#include "torqueEncoder.h"
#include "motorController.h"
#include "powerLimit.h"

//Export macro
#ifdef _WIN32
  #define DLL_EXPORT __declspec(dllexport)
#else
  #define DLL_EXPORT __attribute__((visibility("default")))
#endif

//-----------------TorqueEncoder-----------------//
// typedef struct {
//     int raw0;
//     int raw1;
// } VirtualTPS;

// static VirtualTPS g_vtps;

// DLL_EXPORT void TEST_TPS_set0_raw(TorqueEncoder* te, int raw) { (void)te; g_vtps.raw0 = raw; }
// DLL_EXPORT void TEST_TPS_set1_raw(TorqueEncoder* te, int raw) { (void)te; g_vtps.raw1 = raw; }

// Set TPS percentages directly (0.0 to 1.0)
DLL_EXPORT void TEST_TPS_set0_percent(TorqueEncoder* te, float percent) { 
    te->tps0_percent = percent; 
}

DLL_EXPORT void TEST_TPS_set1_percent(TorqueEncoder* te, float percent) { 
    te->tps1_percent = percent; 
}

// Set both TPS percentages at once (for convenience)
DLL_EXPORT void TEST_TPS_setBoth_percent(TorqueEncoder* te, float percent) { 
    te->tps0_percent = percent;
    te->tps1_percent = percent;
}

// Set Travel Percent
DLL_EXPORT void TEST_TPS_setTravelPercent(TorqueEncoder* te) { 
    te->travelPercent = (te->tps0_percent + te->tps1_percent) / 2; 
}

//-----------------MotorController-----------------//
// Set CommandedTorque with commands_torque (simulates MCM feedback over CAN)
DLL_EXPORT void TEST_MCM_setCommandedTorque(MotorController* me) { 
    MCM_TEST_setCommandedTorque(me, MCM_commands_getTorque(me));
}

// Set DC Voltage increment
DLL_EXPORT void TEST_MCM_setDCVoltage(MotorController* me, sbyte4 voltage) { 
    MCM_incrementVoltageForTesting(me, voltage);
}

// Set DC Current increment
DLL_EXPORT void TEST_MCM_setDCCurrent(MotorController* me, sbyte4 current) { 
    MCM_incrementCurrentForTesting(me, current);
}

// Set Motor RPM increment
DLL_EXPORT void TEST_MCM_setRPM(MotorController* me, sbyte4 motorRPM) { 
    MCM_incrementRPMForTesting(me, motorRPM);
}

//-----------------PowerLimit-----------------//
// Set PLAlwaysOn
DLL_EXPORT void TEST_setPLAlwaysOn(PowerLimit* me, bool alwaysOn) { 
    me->plAlwaysOn = alwaysOn;
}

// Set PID -> kp, ki, kd, saturationValue, scalefactor
DLL_EXPORT void TEST_setPID(PowerLimit* me, sbyte1 kp, sbyte1 ki, sbyte1 kd, sbyte2 saturationValue, sbyte2 scalefactor) { 
    me->pid = PID_new(kp, ki, kd, saturationValue, scalefactor);
}

// Set PLMode
DLL_EXPORT void TEST_setPLMode(PowerLimit* me, ubyte1 mode) { 
    me->plMode = mode;
}

// Set PLStatus
DLL_EXPORT void TEST_setPLStatus(PowerLimit* me, bool status) { 
    me->plStatus = status;
}

// Set PLTorqueCommand
DLL_EXPORT void TEST_setPLTorqueCommand(PowerLimit* me, sbyte2 plTorqueCommand) { 
    me->plTorqueCommand = plTorqueCommand;
}

// Set PLTargetPower
DLL_EXPORT void TEST_setPLTargetPower(PowerLimit* me, ubyte1 targetPower) { 
    me->plTargetPower = targetPower;
}

// Set PLThresholdDiscrepancy
DLL_EXPORT void TEST_setPLThresholdDiscrepancy(PowerLimit* me, ubyte1 thresholdDiscrepancy) { 
    me->plThresholdDiscrepancy = thresholdDiscrepancy;
}

// Set PLInitializationThreshold
DLL_EXPORT void TEST_setPLInitializationThreshold(PowerLimit* me, ubyte1 initializationThreshold) { 
    me->plInitializationThreshold = initializationThreshold;
}

// Set PLClampingMethod
DLL_EXPORT void TEST_setClampingMethod(PowerLimit* me, ubyte1 clampingMethod) { 
    me->clampingMethod = clampingMethod;
}

// Get PLTorqueCommand
DLL_EXPORT sbyte2 TEST_getPLTorqueCommand(PowerLimit* me) { 
    return me->plTorqueCommand;
}

