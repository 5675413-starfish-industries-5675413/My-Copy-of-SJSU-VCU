#ifndef _DERATING_H
#define _DERATING_H

#include "IO_Driver.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DERATING_MOTOR_RTD_SENSOR_COUNT 5

typedef enum
{
    LIMIT_NONE = 0,
    LIMIT_MOTOR = 1,
    LIMIT_INVERTER = 2,
    LIMIT_BATTERY = 3,
    LIMIT_SENSOR_FAULT = 4
} LimitingReason;

typedef struct
{
    float4 warningTempC;
    float4 hardLimitTempC;
} TemperatureLimit;

typedef struct
{
    float4 driveTorqueHardLimitNm;
    float4 regenTorqueHardLimitNm;

    TemperatureLimit motorTempLimit;
    TemperatureLimit inverterTempLimit;
    TemperatureLimit batteryTempLimit;

    float4 torqueLimitRiseRateNmPerS;
    float4 torqueLimitFallRateNmPerS;

    float4 failsafeDriveTorqueLimitNm;
    float4 failsafeRegenTorqueLimitNm;
} DeratingConfig;

typedef struct
{
    float4 requestedTorqueNm;

    float4 motorTempMeasuredC;
    float4 motorRtdTempsC[DERATING_MOTOR_RTD_SENSOR_COUNT];
    float4 inverterModuleATempC;
    float4 inverterModuleBTempC;
    float4 inverterModuleCTempC;
    float4 inverterGateDriverTempC;
    float4 batteryMaxCellTempC;

    float4 coolantInletTempC;
    float4 coolantOutletTempC;

    float4 motorIdA;
    float4 motorIqA;
    float4 dcBusVoltageV;
    float4 dcBusCurrentA;
    float4 motorSpeedRpm;
} DeratingInputs;

typedef struct
{
    float4 estimatedWindingTempC;
    float4 estimatedInverterHotspotTempC;
    float4 estimatedBatteryTempC;
    ubyte1 estimatesAreValid;
} ThermalModelOutputs;

typedef struct
{
    float4 positiveTorqueLimitNm;
    float4 negativeTorqueLimitNm;
} DeratingState;

typedef struct
{
    float4 positiveTorqueLimitNm;
    float4 negativeTorqueLimitNm;

    float4 motorCriticalTempC;
    float4 inverterCriticalTempC;
    float4 batteryCriticalTempC;

    float4 motorDerateFactor;
    float4 inverterDerateFactor;
    float4 batteryDerateFactor;

    LimitingReason limitingReason;
} DeratingOutputs;

void ThermalModel_UpdateStub(const DeratingInputs *inputs, ThermalModelOutputs *outputs);
void Derating_Init(DeratingState *state, const DeratingConfig *config);
void Derating_Update(const DeratingConfig *config,
                     DeratingState *state,
                     const DeratingInputs *inputs,
                     const ThermalModelOutputs *thermalModelOutputs,
                     float4 timestepSeconds,
                     DeratingOutputs *outputs);

#ifdef __cplusplus
}
#endif

#endif
