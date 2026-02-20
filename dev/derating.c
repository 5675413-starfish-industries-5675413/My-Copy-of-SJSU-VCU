#include "derating.h"

#define DERATING_INVALID_TEMPERATURE_C (-1000.0f)

static float4 clampFloat(float4 value, float4 minimum, float4 maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static float4 minimumOfThree(float4 a, float4 b, float4 c)
{
    float4 minimumValue = (a < b) ? a : b;
    return (minimumValue < c) ? minimumValue : c;
}

static ubyte1 isValidTemperatureC(float4 temperatureC)
{
    return (temperatureC > -50.0f && temperatureC < 250.0f) ? 1u : 0u;
}

static float4 maxValidTemperature(float4 firstTemperatureC, float4 secondTemperatureC)
{
    ubyte1 firstIsValid = isValidTemperatureC(firstTemperatureC);
    ubyte1 secondIsValid = isValidTemperatureC(secondTemperatureC);

    if (!firstIsValid && !secondIsValid)
    {
        return DERATING_INVALID_TEMPERATURE_C;
    }
    if (!firstIsValid)
    {
        return secondTemperatureC;
    }
    if (!secondIsValid)
    {
        return firstTemperatureC;
    }

    return (firstTemperatureC > secondTemperatureC) ? firstTemperatureC : secondTemperatureC;
}

static ubyte1 computeLinearDerateFactor(float4 criticalTempC,
                                        TemperatureLimit temperatureLimit,
                                        float4 *derateFactorOut)
{
    float4 warningToHardSpanC;

    if (!derateFactorOut)
    {
        return 0u;
    }

    if (!isValidTemperatureC(criticalTempC))
    {
        *derateFactorOut = 0.0f;
        return 0u;
    }

    if (criticalTempC <= temperatureLimit.warningTempC)
    {
        *derateFactorOut = 1.0f;
        return 1u;
    }

    if (criticalTempC >= temperatureLimit.hardLimitTempC)
    {
        *derateFactorOut = 0.0f;
        return 1u;
    }

    warningToHardSpanC = temperatureLimit.hardLimitTempC - temperatureLimit.warningTempC;
    if (warningToHardSpanC <= 0.1f)
    {
        *derateFactorOut = 0.0f;
        return 1u;
    }

    *derateFactorOut = (temperatureLimit.hardLimitTempC - criticalTempC) / warningToHardSpanC;
    return 1u;
}

static float4 applySlewRateLimit(float4 previousValue,
                                 float4 targetValue,
                                 float4 riseRatePerSecond,
                                 float4 fallRatePerSecond,
                                 float4 timestepSeconds)
{
    float4 maxRiseDelta = riseRatePerSecond * timestepSeconds;
    float4 maxFallDelta = fallRatePerSecond * timestepSeconds;
    float4 requestedDelta = targetValue - previousValue;

    if (requestedDelta > 0.0f)
    {
        if (requestedDelta > maxRiseDelta)
        {
            requestedDelta = maxRiseDelta;
        }
    }
    else
    {
        if (-requestedDelta > maxFallDelta)
        {
            requestedDelta = -maxFallDelta;
        }
    }

    return previousValue + requestedDelta;
}

void ThermalModel_UpdateStub(const DeratingInputs *inputs, ThermalModelOutputs *outputs)
{
    int sensorIndex;
    float4 hottestInverterSensorC;
    float4 hottestMotorRtdC;
    float4 hottestMotorSignalC;

    if (!inputs || !outputs)
    {
        return;
    }

    hottestInverterSensorC = inputs->inverterModuleATempC;
    hottestInverterSensorC = maxValidTemperature(hottestInverterSensorC, inputs->inverterModuleBTempC);
    hottestInverterSensorC = maxValidTemperature(hottestInverterSensorC, inputs->inverterModuleCTempC);
    hottestInverterSensorC = maxValidTemperature(hottestInverterSensorC, inputs->inverterGateDriverTempC);

    hottestMotorRtdC = DERATING_INVALID_TEMPERATURE_C;
    for (sensorIndex = 0; sensorIndex < DERATING_MOTOR_RTD_SENSOR_COUNT; sensorIndex++)
    {
        hottestMotorRtdC = maxValidTemperature(hottestMotorRtdC, inputs->motorRtdTempsC[sensorIndex]);
    }
    hottestMotorSignalC = maxValidTemperature(inputs->motorTempMeasuredC, hottestMotorRtdC);

    outputs->estimatedWindingTempC = hottestMotorSignalC;
    outputs->estimatedInverterHotspotTempC = hottestInverterSensorC;
    outputs->estimatedBatteryTempC = inputs->batteryMaxCellTempC;
    outputs->estimatesAreValid = 1u;
}

void Derating_Init(DeratingState *state, const DeratingConfig *config)
{
    if (!state || !config)
    {
        return;
    }

    state->positiveTorqueLimitNm = config->driveTorqueHardLimitNm;
    state->negativeTorqueLimitNm = -config->regenTorqueHardLimitNm;
}

void Derating_Update(const DeratingConfig *config,
                     DeratingState *state,
                     const DeratingInputs *inputs,
                     const ThermalModelOutputs *thermalModelOutputs,
                     float4 timestepSeconds,
                     DeratingOutputs *outputs)
{
    int sensorIndex;
    float4 hottestMotorRtdC;
    float4 motorCriticalTempC;
    float4 inverterCriticalTempC;
    float4 batteryCriticalTempC;
    float4 motorDerateFactor;
    float4 inverterDerateFactor;
    float4 batteryDerateFactor;
    ubyte1 motorTempIsValid;
    ubyte1 inverterTempIsValid;
    ubyte1 batteryTempIsValid;
    float4 positiveTorqueLimitRawNm;
    float4 negativeTorqueLimitRawNm;

    if (!config || !state || !inputs || !outputs || timestepSeconds <= 0.0f)
    {
        return;
    }

    hottestMotorRtdC = DERATING_INVALID_TEMPERATURE_C;
    for (sensorIndex = 0; sensorIndex < DERATING_MOTOR_RTD_SENSOR_COUNT; sensorIndex++)
    {
        hottestMotorRtdC = maxValidTemperature(hottestMotorRtdC, inputs->motorRtdTempsC[sensorIndex]);
    }

    motorCriticalTempC = inputs->motorTempMeasuredC;
    motorCriticalTempC = maxValidTemperature(motorCriticalTempC, hottestMotorRtdC);
    if (thermalModelOutputs && thermalModelOutputs->estimatesAreValid)
    {
        motorCriticalTempC = maxValidTemperature(motorCriticalTempC, thermalModelOutputs->estimatedWindingTempC);
    }

    inverterCriticalTempC = inputs->inverterModuleATempC;
    inverterCriticalTempC = maxValidTemperature(inverterCriticalTempC, inputs->inverterModuleBTempC);
    inverterCriticalTempC = maxValidTemperature(inverterCriticalTempC, inputs->inverterModuleCTempC);
    inverterCriticalTempC = maxValidTemperature(inverterCriticalTempC, inputs->inverterGateDriverTempC);
    if (thermalModelOutputs && thermalModelOutputs->estimatesAreValid)
    {
        inverterCriticalTempC = maxValidTemperature(inverterCriticalTempC, thermalModelOutputs->estimatedInverterHotspotTempC);
    }

    batteryCriticalTempC = inputs->batteryMaxCellTempC;
    if (thermalModelOutputs && thermalModelOutputs->estimatesAreValid)
    {
        batteryCriticalTempC = maxValidTemperature(batteryCriticalTempC, thermalModelOutputs->estimatedBatteryTempC);
    }

    outputs->motorCriticalTempC = motorCriticalTempC;
    outputs->inverterCriticalTempC = inverterCriticalTempC;
    outputs->batteryCriticalTempC = batteryCriticalTempC;

    motorTempIsValid = computeLinearDerateFactor(motorCriticalTempC, config->motorTempLimit, &motorDerateFactor);
    inverterTempIsValid = computeLinearDerateFactor(inverterCriticalTempC, config->inverterTempLimit, &inverterDerateFactor);
    batteryTempIsValid = computeLinearDerateFactor(batteryCriticalTempC, config->batteryTempLimit, &batteryDerateFactor);

    if (motorTempIsValid)
    {
        outputs->motorDerateFactor = clampFloat(motorDerateFactor, 0.0f, 1.0f);
    }
    else
    {
        outputs->motorDerateFactor = 0.0f;
    }

    if (inverterTempIsValid)
    {
        outputs->inverterDerateFactor = clampFloat(inverterDerateFactor, 0.0f, 1.0f);
    }
    else
    {
        outputs->inverterDerateFactor = 0.0f;
    }

    if (batteryTempIsValid)
    {
        outputs->batteryDerateFactor = clampFloat(batteryDerateFactor, 0.0f, 1.0f);
    }
    else
    {
        outputs->batteryDerateFactor = 0.0f;
    }

    if (!motorTempIsValid || !inverterTempIsValid || !batteryTempIsValid)
    {
        positiveTorqueLimitRawNm = clampFloat(config->failsafeDriveTorqueLimitNm,
                                              0.0f,
                                              config->driveTorqueHardLimitNm);
        negativeTorqueLimitRawNm = -clampFloat(config->failsafeRegenTorqueLimitNm,
                                               0.0f,
                                               config->regenTorqueHardLimitNm);
        outputs->limitingReason = LIMIT_SENSOR_FAULT;
    }
    else
    {
        float4 overallDerateFactor = minimumOfThree(outputs->motorDerateFactor,
                                                    outputs->inverterDerateFactor,
                                                    outputs->batteryDerateFactor);

        positiveTorqueLimitRawNm = config->driveTorqueHardLimitNm * overallDerateFactor;
        negativeTorqueLimitRawNm = -(config->regenTorqueHardLimitNm * overallDerateFactor);

        if (outputs->motorDerateFactor <= outputs->inverterDerateFactor &&
            outputs->motorDerateFactor <= outputs->batteryDerateFactor)
        {
            outputs->limitingReason = (outputs->motorDerateFactor < 0.999f) ? LIMIT_MOTOR : LIMIT_NONE;
        }
        else if (outputs->inverterDerateFactor <= outputs->motorDerateFactor &&
                 outputs->inverterDerateFactor <= outputs->batteryDerateFactor)
        {
            outputs->limitingReason = (outputs->inverterDerateFactor < 0.999f) ? LIMIT_INVERTER : LIMIT_NONE;
        }
        else
        {
            outputs->limitingReason = (outputs->batteryDerateFactor < 0.999f) ? LIMIT_BATTERY : LIMIT_NONE;
        }
    }

    positiveTorqueLimitRawNm = clampFloat(positiveTorqueLimitRawNm, 0.0f, config->driveTorqueHardLimitNm);
    negativeTorqueLimitRawNm = clampFloat(negativeTorqueLimitRawNm, -config->regenTorqueHardLimitNm, 0.0f);

    state->positiveTorqueLimitNm = applySlewRateLimit(state->positiveTorqueLimitNm,
                                                      positiveTorqueLimitRawNm,
                                                      config->torqueLimitRiseRateNmPerS,
                                                      config->torqueLimitFallRateNmPerS,
                                                      timestepSeconds);

    state->negativeTorqueLimitNm = applySlewRateLimit(state->negativeTorqueLimitNm,
                                                      negativeTorqueLimitRawNm,
                                                      config->torqueLimitRiseRateNmPerS,
                                                      config->torqueLimitFallRateNmPerS,
                                                      timestepSeconds);

    outputs->positiveTorqueLimitNm = state->positiveTorqueLimitNm;
    outputs->negativeTorqueLimitNm = state->negativeTorqueLimitNm;
}
