#include <stdint.h>
#include "torqueEncoder.h"
#include "motorController.h"

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

