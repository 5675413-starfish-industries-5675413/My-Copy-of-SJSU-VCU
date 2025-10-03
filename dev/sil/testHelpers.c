#include <stdbool.h>
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

//MotorController

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
DLL_EXPORT int16_t TEST_PL_getTorqueCmd(const PowerLimit* pl)   { return pl->plTorqueCommand; }
DLL_EXPORT bool TEST_PL_getStatus(const PowerLimit* pl)         { return pl->plStatus; }