#include <stdint.h>
#include "powerLimit.h"
#include "motorController.h"
#include "serial.h"
#include "torqueEncoder.h"

//Export macro
#ifdef _WIN32
  #define DLL_EXPORT __declspec(dllexport)
#else
  #define DLL_EXPORT __attribute__((visibility("default")))
#endif

//TorqueEncoder
typedef struct {
    int raw0;
    int raw1;
} VirtualTPS;

static VirtualTPS g_vtps;

DLL_EXPORT void TEST_TPS_set0_raw(TorqueEncoder* te, int raw) { (void)te; g_vtps.raw0 = raw; }
DLL_EXPORT void TEST_TPS_set1_raw(TorqueEncoder* te, int raw) { (void)te; g_vtps.raw1 = raw; }

//MotorController - Test helper implementations using existing functions
DLL_EXPORT void TEST_MCM_setRPM(MotorController* mcm, int rpm) { 
    MCM_incrementRPMForTesting(mcm, rpm - MCM_getMotorRPM(mcm)); 
}

DLL_EXPORT void TEST_MCM_setBusDeciVolts(MotorController* mcm, int dv) { 
    MCM_incrementVoltageForTesting(mcm, dv - MCM_getDCVoltage(mcm)); 
}

DLL_EXPORT void TEST_MCM_setCurrent(MotorController* mcm, int current) { 
    MCM_incrementCurrentForTesting(mcm, current - MCM_getDCCurrent(mcm)); 
}

DLL_EXPORT void TEST_MCM_setCommandedTorque(MotorController* mcm, int torque) { 
    MCM_commands_setTorqueDNm(mcm, torque); 
}

DLL_EXPORT void TEST_MCM_setAppsTorque(MotorController* mcm, int torque) { 
    // For apps torque, we'll simulate by setting TPS values
    // This will be handled in the Python test by calculating appropriate TPS values
}

DLL_EXPORT int TEST_MCM_getRPM(MotorController* mcm) { 
    return MCM_getMotorRPM(mcm); 
}

DLL_EXPORT int TEST_MCM_getBusDeciVolts(MotorController* mcm) { 
    return MCM_getDCVoltage(mcm); 
}

DLL_EXPORT int TEST_MCM_getCurrent(MotorController* mcm) { 
    return MCM_getDCCurrent(mcm); 
}

DLL_EXPORT int TEST_MCM_getCommandedTorque(MotorController* mcm) { 
    return MCM_getCommandedTorque(mcm); 
}

DLL_EXPORT int TEST_MCM_getAppsTorque(MotorController* mcm) { 
    return MCM_commands_getAppsTorque(mcm); 
}

//PowerLimit
DLL_EXPORT void PowerLimit_calculateCommand(PowerLimit *me, MotorController *mcm, TorqueEncoder *tps);

//getter
DLL_EXPORT ubyte1 POWERLIMIT_getClampingMethod(PowerLimit* me);
DLL_EXPORT bool   POWERLIMIT_getStatus(PowerLimit* me);
DLL_EXPORT ubyte1 POWERLIMIT_getMode(PowerLimit* me);
DLL_EXPORT sbyte2 POWERLIMIT_getTorqueCommand(PowerLimit* me);
DLL_EXPORT ubyte1 POWERLIMIT_getTargetPower(PowerLimit* me);
DLL_EXPORT ubyte1 POWERLIMIT_getInitialisationThreshold(PowerLimit* me);

//test helper functions
DLL_EXPORT void TEST_PL_setTargetPower(PowerLimit* pl, uint8_t kw) { pl->plTargetPower = kw; }
DLL_EXPORT void TEST_PL_setMode(PowerLimit* pl, uint8_t mode)   { pl->plMode = mode; }
DLL_EXPORT void TEST_PL_setAlwaysOn(PowerLimit* pl, bool on)    { pl->plAlwaysOn = on; }
DLL_EXPORT void TEST_PL_setToggle(PowerLimit* pl, bool on)      { pl->plToggle = on; }
DLL_EXPORT void TEST_PL_setClampingMethod(PowerLimit* pl, uint8_t method) { pl->clampingMethod = method; }
DLL_EXPORT void TEST_PL_setThresholdDiscrepancy(PowerLimit* pl, uint8_t threshold) { pl->plThresholdDiscrepancy = threshold; }
DLL_EXPORT int16_t TEST_PL_getTorqueCmd(const PowerLimit* pl)   { return pl->plTorqueCommand; }
DLL_EXPORT bool TEST_PL_getStatus(const PowerLimit* pl)         { return pl->plStatus; }
DLL_EXPORT uint8_t TEST_PL_getTargetPower(const PowerLimit* pl) { return pl->plTargetPower; }
DLL_EXPORT uint8_t TEST_PL_getMode(const PowerLimit* pl)        { return pl->plMode; }
DLL_EXPORT bool TEST_PL_getAlwaysOn(const PowerLimit* pl)       { return pl->plAlwaysOn; }
DLL_EXPORT bool TEST_PL_getToggle(const PowerLimit* pl)         { return pl->plToggle; }
DLL_EXPORT uint8_t TEST_PL_getClampingMethod(const PowerLimit* pl) { return pl->clampingMethod; }
DLL_EXPORT uint8_t TEST_PL_getThresholdDiscrepancy(const PowerLimit* pl) { return pl->plThresholdDiscrepancy; }