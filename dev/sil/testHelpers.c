#include <stdint.h>
#include "torqueEncoder.h"
#include "motorController.h"
#include "powerLimit.h"
#include "regen.h"
#include "brakePressureSensor.h"

#ifndef MIN_REGEN_SPEED_KPH
#define MIN_REGEN_SPEED_KPH 5
#endif

static ubyte2 g_minRegenSpeedKPH = MIN_REGEN_SPEED_KPH;

//Export macro
#ifdef _WIN32
  #define DLL_EXPORT __declspec(dllexport)
#else
  #define DLL_EXPORT __attribute__((visibility("default")))
#endif

//-----------------Constructors (Exported Wrappers)-----------------//
// These already exist in the source files, just export them
extern SerialManager* SerialManager_new(void);
extern MotorController* MotorController_new(SerialManager* sm, ubyte2 id, Direction dir, sbyte2 a, sbyte1 b, sbyte1 c);
extern PowerLimit* POWERLIMIT_new(bool plToggle);
extern TorqueEncoder* TorqueEncoder_new(bool bench);

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

// Get tps0_percent
DLL_EXPORT float TEST_getTPS0_percent(TorqueEncoder* te) { 
    return te->tps0_percent;
}

// Get tps1_percent
DLL_EXPORT float TEST_getTPS1_percent(TorqueEncoder* te) { 
    return te->tps1_percent;
}

// Get travelPercent
DLL_EXPORT float TEST_getTravelPercent(TorqueEncoder* te) { 
    return te->travelPercent;
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

// Get DC Voltage
DLL_EXPORT sbyte4 TEST_MCM_getDCVoltage(MotorController* me) { 
    return MCM_getDCVoltage(me);
}

// Get DC Current
DLL_EXPORT sbyte4 TEST_MCM_getDCCurrent(MotorController* me) { 
    return MCM_getDCCurrent(me);
}

// Get Motor RPM
DLL_EXPORT sbyte4 TEST_MCM_getMotorRPM(MotorController* me) { 
    return MCM_getMotorRPM(me);
}

DLL_EXPORT float4 TEST_MCM_getGroundSpeedKPH(MotorController* me) {
    return MCM_getGroundSpeedKPH(me);
}

DLL_EXPORT void TEST_MCM_setGroundSpeedKPH(MotorController* me, float4 groundSpeedKPH) {
    const float4 FD_Ratio = 3.2f;
    const float4 Revolutions = 60.0f;
    const float4 tireCirc = 1.295f;
    const float4 KPH_Unit_Conversion = 1000.0f;
    float4 motorRPM = ((groundSpeedKPH * KPH_Unit_Conversion) / (Revolutions * tireCirc)) * FD_Ratio;
    me->motorRPM = (sbyte4)motorRPM;
}

// Get APPS Torque
DLL_EXPORT sbyte2 TEST_MCM_getAppsTorque(MotorController* me) { 
    return MCM_commands_getAppsTorque(me);
}

DLL_EXPORT ubyte2 TEST_getMinRegenSpeedKPH(void) { //fix this later dont hard code the value
    return g_minRegenSpeedKPH;
}

DLL_EXPORT void TEST_setMinRegenSpeedKPH(ubyte2 minRegenSpeedKPH) { //same for this one
    g_minRegenSpeedKPH = minRegenSpeedKPH;
}

DLL_EXPORT float4 TEST_TorqueEncoder_getOutputPercent(TorqueEncoder* me) { //look into this later
    float4 outputPercent = 0;
    TorqueEncoder_getOutputPercent(me, &outputPercent);
    return outputPercent;
}

DLL_EXPORT void TEST_BPS_setPressure(BrakePressureSensor* me) { //look into this later too its not properly implemented
    BrakePressureSensor_setPressure(me);
}

//-----------------Regen-----------------//
DLL_EXPORT float4 TEST_Regen_getAppsTorque(Regen* me) {
    return me->appsTorque;
}

DLL_EXPORT float4 TEST_Regen_getBpsTorqueNm(Regen* me) {
    return me->bpsTorqueNm;
}

DLL_EXPORT sbyte2 TEST_Regen_getTorqueLimitDNm(Regen* me) {
    return me->torqueLimitDNm;
}

DLL_EXPORT float4 TEST_Regen_getPadMu(Regen* me) {
    return me->padMu;
}

DLL_EXPORT void TEST_Regen_setAppsTorque(Regen* me, float4 appsTorque) {
    me->appsTorque = appsTorque;
}

DLL_EXPORT void TEST_Regen_setBpsTorqueNm(Regen* me, float4 bpsTorqueNm) {
    me->bpsTorqueNm = bpsTorqueNm;
}

DLL_EXPORT void TEST_Regen_setTorqueLimitDNm(Regen* me, sbyte2 torqueLimitDNm) {
    me->torqueLimitDNm = torqueLimitDNm;
}

DLL_EXPORT RegenMode TEST_Regen_getMode(Regen* me) {
    return me->mode;
}

DLL_EXPORT void TEST_Regen_setMode(Regen* me, RegenMode mode) {
    me->mode = mode;
    Regen_updateSettings(me);
}

DLL_EXPORT void TEST_Regen_setPadMu(Regen* me, float4 padMu) {
    me->padMu = padMu;
}

//-----------------Sensors-----------------//
// Set PL Knob sensor value (for getPLMode() in sensors.c)
extern Sensor Sensor_PLKnob;  // Declared in sensors_sil_stubs.c
DLL_EXPORT void TEST_setPLKnobVoltage(sbyte4 voltage) {
    Sensor_PLKnob.sensorValue = voltage;
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

// TESTING IF IT IS PROPERLY SETTING THE VALUES
// Get PLAlwaysOn
DLL_EXPORT bool TEST_getPLAlwaysOn(PowerLimit* me) { 
    return me->plAlwaysOn;
}

// Get PLMode
DLL_EXPORT ubyte1 TEST_getPLMode(PowerLimit* me) { 
    return me->plMode;
}

// Get PLStatus
DLL_EXPORT bool TEST_getPLStatus(PowerLimit* me) { 
    return me->plStatus;
}

// Get PLTargetPower
DLL_EXPORT ubyte1 TEST_getPLTargetPower(PowerLimit* me) { 
    return me->plTargetPower;
}

// Get PLThresholdDiscrepancy
DLL_EXPORT ubyte1 TEST_getPLThresholdDiscrepancy(PowerLimit* me) { 
    return me->plThresholdDiscrepancy;
}

// Get PLInitializationThreshold
DLL_EXPORT ubyte1 TEST_getPLInitializationThreshold(PowerLimit* me) { 
    return me->plInitializationThreshold;
}

// Get PLClampingMethod
DLL_EXPORT ubyte1 TEST_getPLClampingMethod(PowerLimit* me) { 
    return me->clampingMethod;
}