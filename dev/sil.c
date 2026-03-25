/*****************************************************************************
 * sil.c - Software-In-the-Loop (SIL) implementation
 * Handles JSON input/output for SIL testing mode
 *****************************************************************************/

#include "sil.h"

#ifdef SIL_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Platform-specific includes for stdin polling
#ifdef _WIN32
    #include <windows.h>
    #define STDIN_FILENO 0
#else
    #include <sys/select.h>
    #include <unistd.h>
#endif

// JSON parsing includes
#include "cJSON.h"
#include "PID.h"

// JSON buffer size for reading from stdin
#define JSON_BUFFER_SIZE (256 * 1024)

/*****************************************************************************
 * JSON Parsing Functions
 * Parse JSON configuration values into structs
 *****************************************************************************/

/**
 * Helper function to safely get integer value from JSON, skipping if null
 */
static int get_json_int_safe(cJSON* item, const char* key, int* out_value)
{
    cJSON* json_item = cJSON_GetObjectItem(item, key);
    if (json_item == NULL || cJSON_IsNull(json_item)) {
        return 0; // Field not found or null, skip
    }
    if (cJSON_IsNumber(json_item)) {
        *out_value = json_item->valueint;
        return 1; // Success
    }
    return 0; // Invalid type
}

/**
 * Helper function to safely get double value from JSON, skipping if null
 */
static int get_json_double_safe(cJSON* item, const char* key, double* out_value)
{
    cJSON* json_item = cJSON_GetObjectItem(item, key);
    if (json_item == NULL || cJSON_IsNull(json_item)) {
        return 0; // Field not found or null, skip
    }
    if (cJSON_IsNumber(json_item)) {
        *out_value = json_item->valuedouble;
        return 1; // Success
    }
    return 0; // Invalid type
}

/**
 * Helper function to safely get boolean value from JSON, skipping if null
 */
static int get_json_bool_safe(cJSON* item, const char* key, bool* out_value)
{
    cJSON* json_item = cJSON_GetObjectItem(item, key);
    if (json_item == NULL || cJSON_IsNull(json_item)) {
        return 0; // Field not found or null, skip
    }
    if (cJSON_IsBool(json_item)) {
        *out_value = (bool)cJSON_IsTrue(json_item);
        return 1; // Success
    }
    return 0; // Invalid type
}

/**
 * Parse PID nested structure from JSON
 */
static void parse_PID_from_json(PID* pid, cJSON* pid_json)
{
    if (pid == NULL || pid_json == NULL) {
        return;
    }
    
    int int_val;
    
    if (get_json_int_safe(pid_json, "Kp", &int_val)) {
        pid->Kp = (sbyte1)int_val;
    }
    if (get_json_int_safe(pid_json, "Ki", &int_val)) {
        pid->Ki = (sbyte2)int_val;
    }
    if (get_json_int_safe(pid_json, "Kd", &int_val)) {
        pid->Kd = (sbyte1)int_val;
    }
    if (get_json_int_safe(pid_json, "saturation", &int_val)) {
        pid->saturationValue = (sbyte2)int_val;
    }
    if (get_json_int_safe(pid_json, "gain", &int_val)) {
        pid->scalefactor = (sbyte2)int_val;
    }
}

/**
 * Parse PowerLimit struct values from JSON object
 */
static int parse_PowerLimit_from_json(PowerLimit* pl, cJSON* params)
{
    if (pl == NULL || params == NULL) {
        return -1;
    }
    
    int int_val;
    double double_val;
    bool bool_val;
    
    // Parse nested PID structure
    cJSON* pid_json = cJSON_GetObjectItem(params, "pid");
    if (pid_json != NULL && !cJSON_IsNull(pid_json) && pl->pid != NULL) {
        parse_PID_from_json(pl->pid, pid_json);
    }
    
    // Parse simple fields (skip if null)
    if (get_json_bool_safe(params, "plToggle", &bool_val)) {
        pl->plToggle = bool_val;
    }
    if (get_json_bool_safe(params, "plStatus", &bool_val)) {
        pl->plStatus = bool_val;
    }
    if (get_json_int_safe(params, "plMode", &int_val)) {
        pl->plMode = (ubyte1)int_val;
    }
    if (get_json_int_safe(params, "plTargetPower", &int_val)) {
        pl->plTargetPower = (ubyte1)int_val;
    }
    if (get_json_int_safe(params, "plKwLimit", &int_val)) {
        pl->plKwLimit = (ubyte1)int_val;
    }
    if (get_json_int_safe(params, "plInitializationThreshold", &int_val)) {
        pl->plInitializationThreshold = (ubyte1)int_val;
    }
    if (get_json_int_safe(params, "plTorqueCommand", &int_val)) {
        pl->plTorqueCommand = (sbyte2)int_val;
    }
    if (get_json_int_safe(params, "clampingMethod", &int_val)) {
        pl->clampingMethod = (ubyte1)int_val;
    }
    if (get_json_double_safe(params, "previousPower", &double_val)) {
        pl->previousPower = (float)double_val;
    }
    if (get_json_double_safe(params, "powerChangeRate", &double_val)) {
        pl->powerChangeRate = (float)double_val;
    }
    if (get_json_int_safe(params, "settlingCounter", &int_val)) {
        pl->settlingCounter = (ubyte2)int_val;
    }
    if (get_json_bool_safe(params, "usePowerPID", &bool_val)) {
        pl->usePowerPID = bool_val;
    }
    if (get_json_int_safe(params, "SETTLING_THRESHOLD", &int_val)) {
        pl->SETTLING_THRESHOLD = (ubyte2)int_val;
    }
    if (get_json_double_safe(params, "POWER_CHANGE_THRESHOLD", &double_val)) {
        pl->POWER_CHANGE_THRESHOLD = (float)double_val;
    }
    if (get_json_double_safe(params, "POWER_BELOW_SETPOINT_THRESHOLD", &double_val)) {
        pl->POWER_BELOW_SETPOINT_THRESHOLD = (float)double_val;
    }
    if (get_json_int_safe(params, "vFloorRFloor", &int_val)) {
        pl->vFloorRFloor = (ubyte1)int_val;
    }
    if (get_json_int_safe(params, "vFloorRCeiling", &int_val)) {
        pl->vFloorRCeiling = (ubyte1)int_val;
    }
    if (get_json_int_safe(params, "vCeilingRFloor", &int_val)) {
        pl->vCeilingRFloor = (ubyte1)int_val;
    }
    if (get_json_int_safe(params, "vCeilingRCeiling", &int_val)) {
        pl->vCeilingRCeiling = (ubyte1)int_val;
    }
    if (get_json_bool_safe(params, "plAlwaysOn", &bool_val)) {
        pl->plAlwaysOn = bool_val;
    }
    if (get_json_int_safe(params, "plThresholdDiscrepancy", &int_val)) {
        pl->plThresholdDiscrepancy = (ubyte1)int_val;
    }
    
    return 0;
}

/**
 * Parse MotorController struct values from JSON object
 * Uses increment functions from motorController.h to set values
 */
static int parse_MotorController_from_json(MotorController* mcm, cJSON* params)
{
    if (mcm == NULL || params == NULL) {
        return -1;
    }
    
    int int_val;
    
    // Set exact DC Voltage for SIL input (do not clamp to increment helper caps)
    if (get_json_int_safe(params, "DC_Voltage", &int_val)) {
        MCM_setVoltageForTesting(mcm, (sbyte4)int_val);
    }
    
    // Set exact DC Current for SIL input (do not clamp to increment helper caps)
    if (get_json_int_safe(params, "DC_Current", &int_val)) {
        MCM_setCurrentForTesting(mcm, (sbyte4)int_val);
    }
    
    // Set exact Motor RPM for SIL input.
    if (get_json_int_safe(params, "motorRPM", &int_val)) {
        MCM_setRPMForTesting(mcm, (sbyte4)int_val);
    }
    
    return 0;
}

/**
 * Parse TorqueEncoder struct values from JSON object
 */
static int parse_TorqueEncoder_from_json(TorqueEncoder* tps, cJSON* params)
{
    if (tps == NULL || params == NULL) {
        return -1;
    }
    
    int int_val;
    double double_val;
    bool bool_val;
    bool has_tps0_percent = FALSE;
    bool has_tps1_percent = FALSE;
    bool has_travel_percent = FALSE;
    
    if (get_json_int_safe(params, "tps0_calibMin", &int_val)) {
        tps->tps0_calibMin = (ubyte4)int_val;
    }
    if (get_json_int_safe(params, "tps0_calibMax", &int_val)) {
        tps->tps0_calibMax = (ubyte4)int_val;
    }
    if (get_json_bool_safe(params, "tps0_reverse", &bool_val)) {
        tps->tps0_reverse = bool_val;
    }
    if (get_json_double_safe(params, "tps0_percent", &double_val)) {
        tps->tps0_percent = (float4)double_val;
        has_tps0_percent = TRUE;
    }
    if (get_json_int_safe(params, "tps1_calibMin", &int_val)) {
        tps->tps1_calibMin = (ubyte4)int_val;
    }
    if (get_json_int_safe(params, "tps1_calibMax", &int_val)) {
        tps->tps1_calibMax = (ubyte4)int_val;
    }
    if (get_json_bool_safe(params, "tps1_reverse", &bool_val)) {
        tps->tps1_reverse = bool_val;
    }
    if (get_json_double_safe(params, "tps1_percent", &double_val)) {
        tps->tps1_percent = (float4)double_val;
        has_tps1_percent = TRUE;
    }
    if (get_json_double_safe(params, "travelPercent", &double_val)) {
        tps->travelPercent = (float4)double_val;
        has_travel_percent = TRUE;
    }
    // If travelPercent is not explicitly provided, derive it from TPS sensor percents.
    if (!has_travel_percent && (has_tps0_percent || has_tps1_percent)) {
        tps->travelPercent = (tps->tps0_percent + tps->tps1_percent) / 2.0f;
    }
    if (get_json_double_safe(params, "outputCurveExponent", &double_val)) {
        tps->outputCurveExponent = (float4)double_val;
    }
    if (get_json_bool_safe(params, "calibrated", &bool_val)) {
        tps->calibrated = bool_val;
    }
    if (get_json_bool_safe(params, "implausibility", &bool_val)) {
        tps->implausibility = bool_val;
    }
    
    return 0;
}

/**
 * Parse BrakePressureSensor struct values from JSON object
 */
static int parse_BrakePressureSensor_from_json(BrakePressureSensor* bps, cJSON* params)
{
    if (bps == NULL || params == NULL) {
        return -1;
    }

    int int_val;
    double double_val;
    bool bool_val;

    if (get_json_int_safe(params, "bps0_calibMin", &int_val)) {
        bps->bps0_calibMin = (ubyte2)int_val;
    }
    if (get_json_int_safe(params, "bps0_calibMax", &int_val)) {
        bps->bps0_calibMax = (ubyte2)int_val;
    }
    if (get_json_bool_safe(params, "bps0_reverse", &bool_val)) {
        bps->bps0_reverse = bool_val;
    }
    if (get_json_int_safe(params, "bps0_voltage", &int_val)) {
        bps->bps0_voltage = (ubyte2)int_val;
    }

    if (get_json_int_safe(params, "bps1_calibMin", &int_val)) {
        bps->bps1_calibMin = (ubyte2)int_val;
    }
    if (get_json_int_safe(params, "bps1_calibMax", &int_val)) {
        bps->bps1_calibMax = (ubyte2)int_val;
    }
    if (get_json_bool_safe(params, "bps1_reverse", &bool_val)) {
        bps->bps1_reverse = bool_val;
    }
    if (get_json_int_safe(params, "bps1_voltage", &int_val)) {
        bps->bps1_voltage = (ubyte2)int_val;
    }

    if (get_json_bool_safe(params, "runCalibration", &bool_val)) {
        bps->runCalibration = bool_val;
    }
    if (get_json_int_safe(params, "calibrationRunTime", &int_val)) {
        bps->calibrationRunTime = (ubyte1)int_val;
    }
    if (get_json_bool_safe(params, "calibrated", &bool_val)) {
        bps->calibrated = bool_val;
    }
    if (get_json_double_safe(params, "percent", &double_val)) {
        bps->percent = (float4)double_val;
    }
    if (get_json_bool_safe(params, "brakesAreOn", &bool_val)) {
        bps->brakesAreOn = bool_val;
    }
    if (get_json_bool_safe(params, "implausibility", &bool_val)) {
        bps->implausibility = bool_val;
    }

    return 0;
}

/**
 * Parse Regen struct values from JSON object
 */
static int parse_Regen_from_json(Regen* regen, cJSON* params)
{
    if (regen == NULL || params == NULL) {
        return -1;
    }

    int int_val;
    double double_val;
    bool bool_val;

    if (get_json_bool_safe(params, "regenToggle", &bool_val)) {
        regen->regenToggle = bool_val;
    }
    if (get_json_int_safe(params, "mode", &int_val)) {
        regen->mode = (RegenMode)int_val;
    }
    if (get_json_int_safe(params, "torqueLimitDNm", &int_val)) {
        regen->torqueLimitDNm = (sbyte2)int_val;
    }
    if (get_json_double_safe(params, "appsTorque", &double_val)) {
        regen->appsTorque = (float4)double_val;
    }
    if (get_json_double_safe(params, "bpsTorqueNm", &double_val)) {
        regen->bpsTorqueNm = (float4)double_val;
    }
    if (get_json_int_safe(params, "regenTorqueCommand", &int_val)) {
        regen->regenTorqueCommand = (sbyte2)int_val;
    }
    if (get_json_int_safe(params, "torqueAtZeroPedalDNm", &int_val)) {
        regen->torqueAtZeroPedalDNm = (ubyte2)int_val;
    }
    if (get_json_double_safe(params, "percentBPSForMaxRegen", &double_val)) {
        regen->percentBPSForMaxRegen = (float4)double_val;
    }
    if (get_json_double_safe(params, "percentAPPSForCoasting", &double_val)) {
        regen->percentAPPSForCoasting = (float4)double_val;
    }
    if (get_json_double_safe(params, "padMu", &double_val)) {
        regen->padMu = (float4)double_val;
    }
    if (get_json_int_safe(params, "tick", &int_val)) {
        regen->tick = (sbyte1)int_val;
    }

    return 0;
}

static int parse_Efficiency_from_json(Efficiency* eff, cJSON* params) {
    if (eff == NULL || params == NULL) {
        return -1;
    }

    int int_val;
    double double_val;
    bool bool_val;

    if (get_json_bool_safe(params, "efficiencyToggle", &bool_val)) {
        eff->efficiencyToggle = bool_val;
    }
    if (get_json_double_safe(params, "energyBudget_kWh", &double_val)) {
        eff->energyBudget_kWh = (float)double_val;
    }
    if (get_json_double_safe(params, "carryOverEnergy_kWh", &double_val)) {
        eff->carryOverEnergy_kWh = (float)double_val;
    }
    if (get_json_int_safe(params, "lapCounter", &int_val)) {
        eff->lapCounter = (ubyte1)int_val;
    }
    if (get_json_double_safe(params, "straightTime_s", &double_val)) {
        eff->straightTime_s = (float)double_val;
    }
    if (get_json_double_safe(params, "cornerTime_s", &double_val)) {
        eff->cornerTime_s = (float)double_val;
    }
    if (get_json_double_safe(params, "cornerEnergy_kWh", &double_val)) {
        eff->cornerEnergy_kWh = (float)double_val;
    }
    if (get_json_double_safe(params, "straightEnergy_kWh", &double_val)) {
        eff->straightEnergy_kWh = (float)double_val;
    }
    if (get_json_double_safe(params, "lapEnergy_kWh", &double_val)) {
        eff->lapEnergy_kWh = (float)double_val;
    }
    if (get_json_double_safe(params, "lapDistance_km", &double_val)) {
        eff->lapDistance_km = (float)double_val;
    }
    if (get_json_bool_safe(params, "finishedLap", &bool_val)) {
        eff->finishedLap = bool_val;
    }

    return 0;
}
/*
static int parse_bms_from_json(BatteryManagementSystem* bms, cJSON* params) {
    if (bms == NULL || params == NULL) {
        return -1;
    }

    int int_val;
    double double_val;

    if (get_json_double_safe(params, "highestCellVoltage_mV", &double_val)) {
        bms->highestCellVoltage_mV = (float)double_val;
    }
    if (get_json_double_safe(params, "lowestCellVoltage_mV", &double_val)) {
        bms->lowestCellVoltage_mV = (float)double_val;
    }
    if (get_json_double_safe(params, "packVoltage_mV", &double_val)) {
        bms->packVoltage_mV = (float)double_val;
    }
    if (get_json_double_safe(params, "highestCellTemp_d_degC", &double_val)) {
        bms->highestCellTemp_d_degC = (double)double_val;
    }
    if (get_json_double_safe(params, "highestCellTemp_degC", &double_val)) {
        bms->highestCellTemp_degC = (double)double_val;
    }
    if (get_json_double_safe(params, "power_uW", &double_val)) {
        bms->power_uW = (double)double_val;
    }
    if (get_json_double_safe(params, "power_W", &double_val)) {
        bms->power_W = (double)double_val;
    }

    return 0;
}*/ // BMS struct isn't completely defined yet and thats why its giving an error.

static int parse_launchControl_from_json(LaunchControl* lc, cJSON* params) {

    if (lc == NULL || params == NULL) {
        return -1;
    }

    int int_val;
    double double_val;
    bool bool_val;

    cJSON* pid_json = cJSON_GetObjectItem(params, "pid");
    if (pid_json != NULL && !cJSON_IsNull(pid_json) && lc->pid != NULL) {
        parse_PID_from_json(lc->pid, pid_json);
    }

    if (get_json_bool_safe(params, "lcToggle", &bool_val)) {
        lc->lcToggle = bool_val;
    }
    if (get_json_int_safe(params, "Kp", &int_val)) {
        lc->Kp = (sbyte1)int_val;
    }
    if (get_json_int_safe(params, "Ki", &int_val)) {
        lc->Ki = (sbyte1)int_val;
    }
    if (get_json_int_safe(params, "Kd", &int_val)) {
        lc->Kd = (sbyte1)int_val;
    }
    if (get_json_double_safe(params, "currentSlipRatio", &double_val)) {
        lc->currentSlipRatio = (float4)double_val;
    }
    if (get_json_double_safe(params, "slipRatioTarget", &double_val)) {
        lc->slipRatioTarget = (float4)double_val;
    }
    if (get_json_double_safe(params, "currentVelocityDifference", &double_val)) {
        lc->currentVelocityDifference = (float4)double_val;
    }
    if (get_json_double_safe(params, "targetVelocityDifference", &double_val)) {
        lc->targetVelocityDifference = (float4)double_val;
    }
    if (get_json_int_safe(params, "lcTorqueCommand", &int_val)) {
        lc->lcTorqueCommand = (sbyte2)int_val;
    }
    if (get_json_double_safe(params, "initialTorque", &double_val)) {
        lc->initialTorque = (float4)double_val;
    }
    if (get_json_double_safe(params, "maxTorque", &double_val)) {
        lc->maxTorque = (float4)double_val;
    }
    if (get_json_double_safe(params, "prevTorque", &double_val)) {
        lc->prevTorque = (float4)double_val;
    }
    if (get_json_double_safe(params, "k", &double_val)) {
        lc->k = (float4)double_val;
    }
    if (get_json_bool_safe(params, "useFilter", &bool_val)) {
        lc->useFilter = bool_val;
    }
    if (get_json_int_safe(params, "state", &int_val)) {
        lc->state = (LC_State)int_val;
    }
    if (get_json_int_safe(params, "mode", &int_val)) {
        lc->mode = (LC_Mode)int_val;
    }
    if (get_json_int_safe(params, "phase", &int_val)) {
        lc->phase = (LC_Phase)int_val;
    }

    return 0;

    
}

static int parse_pid_from_json(PID* pid, cJSON* params) {
    if (pid == NULL || params == NULL) {
        return -1;
    }

    int int_val;
    double double_val;
    bool bool_val;

    if (get_json_double_safe(params, "Kp", &double_val)) {
        pid->Kp = (sbyte1)double_val;
    }
    if (get_json_double_safe(params, "Ki", &double_val)) {
        pid->Ki = (sbyte2)double_val;
    }
    if (get_json_double_safe(params, "Kd", &double_val)) {
        pid->Kd = (sbyte1)double_val;
    }
    if (get_json_double_safe(params, "setPoint", &double_val)) {
        pid->setpoint = (float)double_val;
    }
    if (get_json_double_safe(params, "previousError", &double_val)) {
        pid->previousError = (float)double_val;
    }
    if (get_json_double_safe(params, "totalError", &double_val)) {
        pid->totalError = (float)double_val;
    }
    if (get_json_double_safe(params, "output", &double_val)) {
        pid->output = (float)double_val;
    }
    if (get_json_double_safe(params, "proportional", &double_val)) {
        pid->proportional = (float)double_val;
    }
    if (get_json_double_safe(params, "integral", &double_val)) {
        pid->integral = (float)double_val;
    }
    if (get_json_double_safe(params, "derivative", &double_val)) {
        pid->derivative = (float)double_val;
    }
    if (get_json_double_safe(params, "saturationValue", &double_val)) {
        pid->saturationValue = (sbyte2)double_val;
    }
    if (get_json_bool_safe(params, "antiWindupFlag", &bool_val)) {
        pid->antiWindupFlag = bool_val;
    }

    return 0;
}

static int parse_sensor_from_json(Sensor* sensor, cJSON* params) {
    if (sensor == NULL || params == NULL) {
        return -1;
    }

    int int_val;
    double double_val;
    bool bool_val;

    if (get_json_double_safe(params, "specMax", &double_val)) {
        sensor->specMax = (ubyte4)double_val;
    }
    if (get_json_double_safe(params, "sensorValue", &double_val)) {
        sensor->sensorValue = (ubyte4)double_val;
    }
    if (get_json_double_safe(params, "heldSensorValue", &double_val)) {
        sensor->heldSensorValue = (ubyte4)double_val;
    }
    if (get_json_double_safe(params, "timestamp", &double_val)) {
        sensor->timestamp = (ubyte4)double_val;
    }
    if (get_json_bool_safe(params, "fresh", &bool_val)) {
        sensor->fresh = bool_val;
    }
    if (get_json_double_safe(params, "ioErr_powerInit", &double_val)) {
        sensor->ioErr_powerInit = (IO_ErrorType)double_val;
    }
    if (get_json_double_safe(params, "ioErr_powerSet", &double_val)) {
        sensor->ioErr_powerSet = (IO_ErrorType)double_val;
    }
    if (get_json_double_safe(params, "ioErr_signalInit", &double_val)) {
        sensor->ioErr_signalInit = (IO_ErrorType)double_val;
    }
    if (get_json_double_safe(params, "ioErr_signalGet", &double_val)) {
        sensor->ioErr_signalGet = (IO_ErrorType)double_val;
    }

    return 0;
}

static int parse_watchdog_from_json(WatchDog* wd, cJSON* params) {
    if (wd == NULL || params == NULL) {
        return -1;
    }

    int int_val;
    double double_val;
    bool bool_val;

    if (get_json_double_safe(params, "timestamp", &double_val)) {
        wd->timestamp = (ubyte4)double_val;
    }
    if (get_json_double_safe(params, "timeout", &double_val)) {
        wd->timeout = (ubyte4)double_val;
    }
    if (get_json_double_safe(params, "running", &double_val)) {
        wd->running = double_val;
    }

    return 0;
}

static int parse_WheelSpeeds_from_json(WheelSpeeds* wss, cJSON* params)
{
    if (wss == NULL || params == NULL) {
        return -1;
    }

    int int_val;

    if (get_json_int_safe(params, "frequency_FL", &int_val)) {
        Sensor_WSS_FL.sensorValue = (ubyte4)int_val;
        Sensor_WSS_FL.heldSensorValue = (ubyte4)int_val;
    }
    if (get_json_int_safe(params, "frequency_FR", &int_val)) {
        Sensor_WSS_FR.sensorValue = (ubyte4)int_val;
        Sensor_WSS_FR.heldSensorValue = (ubyte4)int_val;
    }
    if (get_json_int_safe(params, "frequency_RL", &int_val)) {
        Sensor_WSS_RL.sensorValue = (ubyte4)int_val;
        Sensor_WSS_RL.heldSensorValue = (ubyte4)int_val;
    }
    if (get_json_int_safe(params, "frequency_RR", &int_val)) {
        Sensor_WSS_RR.sensorValue = (ubyte4)int_val;
        Sensor_WSS_RR.heldSensorValue = (ubyte4)int_val;
    }

    return 0;
}

/**
 * Parse JSON string and assign values to structs
 * This is the main function used by sil_read_initial_json and sil_read_json_input
 */
static int parse_struct_values_from_string(const char* json_string, 
                                           PowerLimit* pl, 
                                           MotorController* mcm, 
                                           TorqueEncoder* tps,
                                           BrakePressureSensor* bps,
                                           Regen* regen,
                                           Efficiency* eff,
                                           BatteryManagementSystem* bms,
                                           LaunchControl* lc,
                                           PID* pid,
                                           Sensor* sensor,
                                           WatchDog* wd,
                                           WheelSpeeds* wss)
{
    if (json_string == NULL) {
        return -1; // Invalid input
    }
    
    // Parse JSON
    cJSON* json = cJSON_Parse(json_string);
    if (json == NULL) {
        return -1; // Failed to parse JSON
    }
    
    // Handle two formats:
    // 1. Direct array: [{...}, {...}]
    // 2. Object with "structs" key: {"structs": [{...}, {...}], "row_id": ...}
    cJSON* structs_array = NULL;
    if (cJSON_IsArray(json)) {
        // Format 1: Direct array
        structs_array = json;
    } else if (cJSON_IsObject(json)) {
        // Format 2: Object with "structs" key
        structs_array = cJSON_GetObjectItem(json, "structs");
        if (structs_array == NULL || !cJSON_IsArray(structs_array)) {
            cJSON_Delete(json);
            return -2; // "structs" key not found or not an array
        }
    } else {
        cJSON_Delete(json);
        return -3; // Invalid format (not array or object)
    }
    
    // Iterate through array to find the structs we need
    int array_size = cJSON_GetArraySize(structs_array);
    for (int i = 0; i < array_size; i++) {
        cJSON* struct_item = cJSON_GetArrayItem(structs_array, i);
        if (struct_item == NULL) {
            continue;
        }
        
        cJSON* name_item = cJSON_GetObjectItem(struct_item, "name");
        if (name_item == NULL || !cJSON_IsString(name_item)) {
            continue;
        }
        
        const char* struct_name = name_item->valuestring;
        cJSON* params = cJSON_GetObjectItem(struct_item, "parameters");
        
        if (params == NULL) {
            continue;
        }
        
        // Parse based on struct name
        if (strcmp(struct_name, "PowerLimit") == 0 && pl != NULL) {
            parse_PowerLimit_from_json(pl, params);
        } else if (strcmp(struct_name, "MotorController") == 0 && mcm != NULL) {
            parse_MotorController_from_json(mcm, params);
        } else if (strcmp(struct_name, "TorqueEncoder") == 0 && tps != NULL) {
            parse_TorqueEncoder_from_json(tps, params);
        } else if (strcmp(struct_name, "BrakePressureSensor") == 0 && bps != NULL) {
            parse_BrakePressureSensor_from_json(bps, params);
        } else if (strcmp(struct_name, "Regen") == 0 && regen != NULL) {
            parse_Regen_from_json(regen, params);
        }
        else if (strcmp(struct_name, "Efficiency") == 0 && eff != NULL) {
            parse_Efficiency_from_json(eff, params);
        }
        /*else if (strcmp(struct_name, "BatteryManagementSystem") == 0 && bms != NULL) {
            parse_bms_from_json(bms, params);
        }*/
        else if (strcmp(struct_name, "LaunchControl") == 0 && lc != NULL) {
            parse_launchControl_from_json(lc, params);
        }
        else if (strcmp(struct_name, "PID") == 0 && pid != NULL) {
            parse_pid_from_json(pid, params);
        }
        else if (strcmp(struct_name, "Sensor") == 0 && sensor != NULL) {
            parse_sensor_from_json(sensor, params);
        }
        else if (strcmp(struct_name, "WatchDog") == 0 && wd != NULL) {
            parse_watchdog_from_json(wd, params);
        }
        else if (strcmp(struct_name, "WheelSpeeds") == 0 && wss != NULL) {
            parse_WheelSpeeds_from_json(wss, params);
        }
    }
    
    cJSON_Delete(json);
    return 0;
}

/**
 * Helper function to read a complete JSON object from stdin
 * @param json_buffer Buffer to store JSON (must be at least JSON_BUFFER_SIZE)
 * @return Number of bytes read, or -1 on error
 */
static int read_json_from_stdin(char* json_buffer)
{
    size_t total_read = 0;
    int brace_count = 0;
    int in_string = 0;
    int escape_next = 0;
    
    // Read until we have a complete JSON object
    while (total_read < JSON_BUFFER_SIZE - 1) {
        int c = fgetc(stdin);
        if (c == EOF) {
            break;
        }
        
        json_buffer[total_read++] = (char)c;
        
        // Track braces to know when JSON is complete
        if (!escape_next && c == '"' && total_read > 1 && json_buffer[total_read-2] != '\\') {
            in_string = !in_string;
        }
        escape_next = (!escape_next && c == '\\' && in_string);
        
        if (!in_string) {
            if (c == '{') brace_count++;
            else if (c == '}') {
                brace_count--;
                if (brace_count == 0) {
                    // Complete JSON object found
                    break;
                }
            }
        }
    }
    
    json_buffer[total_read] = '\0';
    
    // Remove trailing newline if present
    if (total_read > 0 && json_buffer[total_read - 1] == '\n') {
        json_buffer[total_read - 1] = '\0';
        total_read--;
    }
    
    return (int)total_read;
}

int sil_read_initial_json(PowerLimit* pl, MotorController* mcm, TorqueEncoder* tps, BrakePressureSensor* bps, Regen* regen, PID* pid, LaunchControl* lc, WheelSpeeds* wss, Sensor* sensor, WatchDog* wd, Efficiency* eff, BatteryManagementSystem* bms)
{
    char* json_buffer = (char*)malloc(JSON_BUFFER_SIZE);
    if (json_buffer == NULL) {
        fprintf(stderr, "SIL: Failed to allocate JSON buffer\n");
        fflush(stderr);
        return -1;
    }
    
    int total_read = read_json_from_stdin(json_buffer);
    int result = -1;
    
    if (total_read > 0) {
        result = parse_struct_values_from_string(json_buffer, pl, mcm, tps, bps, regen, eff, bms, lc, pid, sensor, wd, wss);
        
        if (result != 0) {
            fprintf(stderr, "SIL: Failed to parse JSON from stdin (error: %d, length: %d)\n",
                    result, total_read);
            fprintf(stderr, "SIL: Received JSON (first 200 chars): %.200s\n", json_buffer);
            if (total_read > 200) {
                fprintf(stderr, "SIL: Received JSON (last 200 chars): %.200s\n", 
                        json_buffer + total_read - 200);
            }
            fflush(stderr);
        }
    } else {
        fprintf(stderr, "SIL: No JSON data read from stdin\n");
        fflush(stderr);
    }
    
    free(json_buffer);
    return result;
}

int sil_read_json_input(PowerLimit* pl, MotorController* mcm, TorqueEncoder* tps, BrakePressureSensor* bps, Regen* regen, PID* pid, LaunchControl* lc, WheelSpeeds* wss, Sensor* sensor, WatchDog* wd, Efficiency* eff, BatteryManagementSystem* bms)
{
    // Non-blocking check for stdin data
    #ifdef _WIN32
    HANDLE stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
    DWORD bytes_available = 0;
    if (stdin_handle == NULL || stdin_handle == INVALID_HANDLE_VALUE ||
        !PeekNamedPipe(stdin_handle, NULL, 0, NULL, &bytes_available, NULL) ||
        bytes_available == 0) {
        return 1;  // No data available
    }
    #else
    fd_set readfds;
    struct timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout) <= 0 || 
        !FD_ISSET(STDIN_FILENO, &readfds)) {
        return 1;  // No data available
    }
    #endif
    
    // Data available, read JSON
    char* json_buffer = (char*)malloc(JSON_BUFFER_SIZE);
    if (json_buffer == NULL) {
        return -1;
    }
    
    int total_read = read_json_from_stdin(json_buffer);
    int parse_result = -1;
    
    if (total_read > 0) {
        parse_result = parse_struct_values_from_string(json_buffer, pl, mcm, tps, bps, regen, eff, bms, lc, pid, sensor, wd, wss);
        if (parse_result != 0) {
            fprintf(stderr, "SIL: Failed to parse JSON in loop (error: %d)\n", parse_result);
            fflush(stderr);
        }
    }
    
    free(json_buffer);
    return parse_result;
}

// Static variable to store output mode (set via compile-time define SIL_OUTPUT_MODE_CONFIG)
// static ubyte1 sil_output_mode = SIL_OUTPUT_MODE_CONFIG;



// NOTE: cJSON ONLY ACCEPTS DOUBLE DATA TYPES FOR NUMBER VALUES
void sil_write_json_output(MotorController* mcm, PowerLimit* pl, Efficiency* eff, BatteryManagementSystem* bms, LaunchControl* lc, BrakePressureSensor* bps, PID* pid, Regen *regen, InstrumentCluster *ic, ReadyToDriveSound *rtds, SafetyChecker *sc, Sensor *sensor, TorqueEncoder *tps, WatchDog *wd, ubyte1 output_mode) {
    
        cJSON *root = cJSON_CreateObject();
        if (root == NULL) {
            return ;
        }
        if (mcm != NULL) {
            cJSON *mcm_json = cJSON_CreateObject();
            if (mcm_json != NULL) {
                cJSON_AddNumberToObject(mcm_json, "motorRPM", (double) MCM_getMotorRPM(mcm));
                cJSON_AddNumberToObject(mcm_json, "DC_Voltage", (double) MCM_getDCVoltage(mcm));
                cJSON_AddNumberToObject(mcm_json, "DC_Current", (double) MCM_getDCCurrent(mcm));
                cJSON_AddNumberToObject(mcm_json, "power_kw", (double) (MCM_getDCVoltage(mcm) * MCM_getDCCurrent(mcm)) / 1000);
                cJSON_AddNumberToObject(mcm_json, "commandedTorque", (double) MCM_getCommandedTorque(mcm) / 10.0f);
                cJSON_AddNumberToObject(mcm_json, "appsTorque", (double) MCM_commands_getAppsTorque(mcm) / 10.0f);
                cJSON_AddNumberToObject(mcm_json, "plTorqueCommand", (double) MCM_get_PL_torqueCommand(mcm) / 10.0f);
                cJSON_AddNumberToObject(mcm_json, "regenTorqueCommand", (double) MCM_get_Regen_torqueCommand(mcm) / 10.0f);
                cJSON_AddNumberToObject(mcm_json, "maxTorque", (double) MCM_getMaxTorqueDNm(mcm) / 10.0f);
                cJSON_AddNumberToObject(mcm_json, "ground_speed_kph", (double) MCM_getGroundSpeedKPH(mcm));
                cJSON_AddItemToObject(root, "MotorController", mcm_json);
            }
        }
        if (pl != NULL) {
            cJSON *pl_json = cJSON_CreateObject();
            if (pl_json != NULL) {
                cJSON_AddBoolToObject(pl_json, "plStatus", pl->plStatus);
                cJSON_AddNumberToObject(pl_json, "plMode", (double) POWERLIMIT_getMode(pl));
                cJSON_AddNumberToObject(pl_json, "plTargetPower", (double) POWERLIMIT_getTargetPower(pl));
                cJSON_AddNumberToObject(pl_json, "plInitializationThreshold", (double) POWERLIMIT_getInitialisationThreshold(pl));
                cJSON_AddNumberToObject(pl_json, "plTorqueCommand", (double) POWERLIMIT_getTorqueCommand (pl)/ 10.0f);
                cJSON_AddNumberToObject(pl_json, "clampingMethod", (double) POWERLIMIT_getClampingMethod(pl));
                cJSON_AddBoolToObject(pl_json, "plAlwaysOn", pl->plAlwaysOn);
                if (mcm != NULL) {
                    cJSON_AddNumberToObject(pl_json, "current_power_kw", (MCM_getDCVoltage(mcm) * MCM_getDCCurrent(mcm)) / 1000);
                }
                cJSON_AddItemToObject(root, "PowerLimit", pl_json);
            }
        }
        if (eff != NULL) {

            cJSON *eff_json = cJSON_CreateObject();
            if (eff_json != NULL) {
                cJSON_AddBoolToObject(eff_json, "efficiencyToggle", eff->efficiencyToggle);
                cJSON_AddNumberToObject(eff_json, "energyBudget_kWh", (double) Efficiency_getEnergyBudget_kWh(eff));
                cJSON_AddNumberToObject(eff_json, "carryOverEnergy_kWh", (double) Efficiency_getCarryOverEnergy_kWh(eff));
                cJSON_AddNumberToObject(eff_json, "lapCounter", (double) Efficiency_getLapCounter(eff));
                cJSON_AddNumberToObject(eff_json, "straightTime_s", (double) Efficiency_getStraightTime_s(eff));
                cJSON_AddNumberToObject(eff_json, "cornerTime_s", (double) eff->cornerTime_s);
                cJSON_AddNumberToObject(eff_json, "cornerEnergy_kWh", (double) Efficiency_getCornerEnergy_kWh(eff));
                cJSON_AddNumberToObject(eff_json, "straightEnergy_kWh", (double) Efficiency_getStraightEnergy_kWh(eff));
                if (pl != NULL) {
                    cJSON_AddNumberToObject(eff_json, "plTargetPower", (double) POWERLIMIT_getTargetPower(pl));
                }
                cJSON_AddNumberToObject(eff_json, "lapEnergy_kWh", (double) Efficiency_getLapEnergy_kWh(eff));
                cJSON_AddNumberToObject(eff_json, "energyBudget_kWh", (double) Efficiency_getEnergyBudget_kWh(eff));
                cJSON_AddNumberToObject(eff_json, "carryOverEnergy_kWh", (double) Efficiency_getCarryOverEnergy_kWh(eff));
                cJSON_AddItemToObject(root, "Efficiency", eff_json);
            }
        }
        if (bms != NULL) {
            cJSON *bms_json = cJSON_CreateObject();
            if (bms_json != NULL) {
                cJSON_AddNumberToObject(bms_json, "highestCellVoltage_mV", (double) BMS_getHighestCellVoltage_mV(bms));
                cJSON_AddNumberToObject(bms_json, "lowestCellVoltage_mV", (double) BMS_getLowestCellVoltage_mV(bms));
                cJSON_AddNumberToObject(bms_json, "packVoltage_mV", (double) BMS_getPackVoltage(bms));
                cJSON_AddNumberToObject(bms_json, "highestCellTemp_d_degC", (double) BMS_getHighestCellTemp_d_degC(bms));
                cJSON_AddNumberToObject(bms_json, "highestCellTemp_degC", (double) BMS_getHighestCellTemp_degC(bms));
                cJSON_AddNumberToObject(bms_json, "power_uW", (double) BMS_getPower_uW(bms));
                cJSON_AddNumberToObject(bms_json, "power_W", (double) BMS_getPower_W(bms));
                cJSON_AddItemToObject(root, "BatteryManagementSystem", bms_json);
            }
        }
        if (lc != NULL) {
            cJSON *lc_json = cJSON_CreateObject();
            if (lc_json != NULL) {
                cJSON_AddBoolToObject(lc_json, "state", (double) LaunchControl_getState(lc));
                cJSON_AddNumberToObject(lc_json, "phase", (double) LaunchControl_getPhase(lc));
                cJSON_AddNumberToObject(lc_json, "torqueCommand", (double) LaunchControl_getTorqueCommand(lc));
                cJSON_AddNumberToObject(lc_json, "slipRatio", (double) LaunchControl_getSlipRatio(lc));
                cJSON_AddNumberToObject(lc_json, "slipRatioScaled", (double) LaunchControl_getSlipRatioScaled(lc));
                cJSON_AddNumberToObject(lc_json, "pidOutput", (double) lc->pidOutput);
                cJSON_AddBoolToObject(lc_json, "initialCurveStatus", LaunchControl_getInitialCurveStatus(lc));
                cJSON_AddNumberToObject(lc_json, "phase", (double) LaunchControl_getPhase(lc));
                cJSON_AddBoolToObject(lc_json, "filterStatus", LaunchControl_getFilterStatus(lc));
                cJSON_AddNumberToObject(lc_json, "velocityDifferenceTarget", (double) LaunchControl_getVelocityDifferenceTarget(lc));
                cJSON_AddNumberToObject(lc_json, "currentVelocityDifference", (double) LaunchControl_getCurrentVelocityDifference(lc));
                cJSON_AddItemToObject(root, "LaunchControl", lc_json);
            }
        }
        if (bps != NULL) {
            cJSON *bps_json = cJSON_CreateObject();
            if (bps_json != NULL) {
                    cJSON_AddNumberToObject(bps_json, "BPS0_V", (double) BrakePressureSensor_getBPS0_Voltage(bps));
                    cJSON_AddNumberToObject(bps_json, "BPS1_V", (double) BrakePressureSensor_getBPS1_Voltage(bps));
               cJSON_AddNumberToObject(bps_json, "BPS0_Pressure", (double) BrakePressureSensor_getBPS0_Pressure(bps));
               cJSON_AddNumberToObject(bps_json, "BPS1_Pressure", (double) BrakePressureSensor_getBPS1_Pressure(bps));
               cJSON_AddItemToObject(root, "BrakePressureSensor", bps_json);
            }
        }
        if (pid != NULL) {
            cJSON *pid_json = cJSON_CreateObject();
            if (pid_json != NULL) {
                cJSON_AddNumberToObject(pid_json, "Kp", (double) PID_getKp(pid));
                cJSON_AddNumberToObject(pid_json, "Ki", (double) PID_getKi(pid));
                cJSON_AddNumberToObject(pid_json, "Kd", (double) PID_getKd(pid));
                cJSON_AddNumberToObject(pid_json, "setPoint", (double) PID_getSetpoint(pid));
                cJSON_AddNumberToObject(pid_json, "previousError", (double) PID_getPreviousError(pid));
                cJSON_AddNumberToObject(pid_json, "totalError", (double) PID_getTotalError(pid));
                cJSON_AddNumberToObject(pid_json, "output", (double) PID_getOutput(pid));
                cJSON_AddNumberToObject(pid_json, "proportional", (double) PID_getProportional(pid));
                cJSON_AddNumberToObject(pid_json, "integral", (double) PID_getIntegral(pid));
                cJSON_AddNumberToObject(pid_json, "derivative", (double) PID_getDerivative(pid));
                cJSON_AddNumberToObject(pid_json, "saturationValue", (double) PID_getSaturationValue(pid));
                cJSON_AddBoolToObject(pid_json, "antiWindupFlag", (double) PID_getAntiWindupFlag(pid));
                cJSON_AddItemToObject(root, "PID", pid_json);
            }
        }
        if (regen != NULL) {
            cJSON *regen_json = cJSON_CreateObject();
            if (regen_json != NULL) {
                cJSON_AddBoolToObject(regen_json, "regenToggle", regen->regenToggle);
                cJSON_AddNumberToObject(regen_json, "mode", (double) regen->mode);
                cJSON_AddNumberToObject(regen_json, "torqueLimitDNm", regen->torqueLimitDNm);
                cJSON_AddNumberToObject(regen_json, "appsTorque", (double) regen->appsTorque);
                cJSON_AddNumberToObject(regen_json, "bpsTorqueNm", (double) regen->bpsTorqueNm);
                cJSON_AddNumberToObject(regen_json, "regenTorqueCommand", (double) Regen_get_torqueCommand(regen));
                cJSON_AddNumberToObject(regen_json, "torqueAtZeroPedalDNm", (double) regen->torqueAtZeroPedalDNm);
                cJSON_AddNumberToObject(regen_json, "percentBPSForMaxRegen", (double) regen->percentBPSForMaxRegen);
                cJSON_AddNumberToObject(regen_json, "percentAPPSForCoasting", (double) regen->percentAPPSForCoasting);
                cJSON_AddNumberToObject(regen_json, "padMu", (double) regen->padMu);
                cJSON_AddNumberToObject(regen_json, "tick", (double) regen->tick);
                cJSON_AddItemToObject(root, "Regen", regen_json);
                
            }
        }
        
        if (ic != NULL) {
            cJSON *ic_json = cJSON_CreateObject();
            if (ic_json != NULL) {
                // cJSON_AddNumberToObject(ic_json, "serialMan", (double) ic->serialMan);
                // cJSON_AddNumberToObject(ic_json, "canMessageBaseId", (double) ic->canMessageBaseId);
                cJSON_AddNumberToObject(ic_json, "torqueMapMode", (double) IC_getTorqueMapMode(ic));
                cJSON_AddNumberToObject(ic_json, "launchControlSensitivity", (double) IC_getLaunchControlSensitivity(ic));
                cJSON_AddItemToObject(root, "InstrumentCluster", ic_json);
            }
        } 
        if (sensor != NULL) {
            cJSON *sensor_json = cJSON_CreateObject();
            if (sensor_json != NULL) {
                // cJSON_AddNumberToObject(sensor_json, "specMin", (double) sensor->specMin); compiler says this doesn't exist, but it's defined in the header file. We can add this back in once we resolve this issue.
                cJSON_AddNumberToObject(sensor_json, "specMax", (double) sensor->specMax);
                cJSON_AddNumberToObject(sensor_json, "sensorValue", (double) sensor->sensorValue);
                cJSON_AddNumberToObject(sensor_json, "heldSensorValue", (double) sensor->heldSensorValue);
                cJSON_AddNumberToObject(sensor_json, "timestamp", (double) sensor->timestamp);
                cJSON_AddBoolToObject(sensor_json, "fresh", sensor->fresh);
                cJSON_AddNumberToObject(sensor_json, "ioErr_powerInit", (double) sensor->ioErr_powerInit);
                cJSON_AddNumberToObject(sensor_json, "ioErr_powerSet", (double) sensor->ioErr_powerSet);
                cJSON_AddNumberToObject(sensor_json, "ioErr_signalInit", (double) sensor->ioErr_signalInit);
                cJSON_AddNumberToObject(sensor_json, "ioErr_signalGet", (double) sensor->ioErr_signalGet);
                cJSON_AddItemToObject(root, "Sensor", sensor_json);
            }
        }
        if (tps != NULL) {
            cJSON *tps_json = cJSON_CreateObject();
            if (tps_json != NULL) {
                cJSON_AddBoolToObject(tps_json, "bench", tps->bench);
                // Emit explicit per-sensor keys to avoid duplicate-key overwrites.
                cJSON_AddNumberToObject(tps_json, "tps0_calibMin", (double) tps->tps0_calibMin);
                cJSON_AddNumberToObject(tps_json, "tps0_calibMax", (double) tps->tps0_calibMax);
                cJSON_AddNumberToObject(tps_json, "tps0_reverse", (double) tps->tps0_reverse);
                cJSON_AddNumberToObject(tps_json, "tps0_value", (double) tps->tps0_value);
                cJSON_AddNumberToObject(tps_json, "tps0_percent", (double) tps->tps0_percent);
                cJSON_AddNumberToObject(tps_json, "tps1_calibMin", (double) tps->tps1_calibMin);
                cJSON_AddNumberToObject(tps_json, "tps1_calibMax", (double) tps->tps1_calibMax);
                cJSON_AddNumberToObject(tps_json, "tps1_reverse", (double) tps->tps1_reverse);
                cJSON_AddNumberToObject(tps_json, "tps1_value", (double) tps->tps1_value);
                cJSON_AddNumberToObject(tps_json, "tps1_percent", (double) tps->tps1_percent);
                cJSON_AddNumberToObject(tps_json, "runCalibration", (double) tps->runCalibration);
                cJSON_AddNumberToObject(tps_json, "timestamp_calibrationStart", (double) tps->timestamp_calibrationStart);
                cJSON_AddNumberToObject(tps_json, "calibrationRunTime", (double) tps->calibrationRunTime);
                cJSON_AddNumberToObject(tps_json, "outputCurveExponent", (double) tps->outputCurveExponent);
                cJSON_AddNumberToObject(tps_json, "calibrated", (double) tps->calibrated);
                cJSON_AddNumberToObject(tps_json, "travelPercent", (double) tps->travelPercent);
                cJSON_AddNumberToObject(tps_json, "specMin", (double) tps->implausibility);
                
                cJSON_AddItemToObject(root, "TorqueEncoder", tps_json);
            }
        }
        if (wd != NULL) {
            cJSON *wd_json = cJSON_CreateObject();
            if (wd_json != NULL) {
                cJSON_AddNumberToObject(wd_json, "timestamp", (double) wd->timestamp);
                cJSON_AddNumberToObject(wd_json, "timeout", (double) wd->timeout);
                cJSON_AddNumberToObject(wd_json, "running", (double) wd->running);
                cJSON_AddItemToObject(root, "WatchDog", wd_json);
            }
        }
        
        
        
        char* json_output = cJSON_PrintUnformatted(root);
        if (json_output != NULL) {
            printf("%s\n", json_output);
            fflush(stdout);
            free(json_output);
        }
        cJSON_Delete(root);

    
    
}
/*
void sil_write_json_output(MotorController* mcm, PowerLimit* pl, Efficiency* eff, ubyte1 output_mode)
{
    int first_field = 1;  // Track if this is the first field in JSON
    
    printf("{");

    if (eff != NULL) {
        if (!first_field) printf(",");
        printf("\"efficiency\":{");
        int first_eff = 1;

        if (mcm != NULL) {
            if (!first_eff) printf(",");
            printf("\"mcm_motorRPM\":%.4f", (float4)MCM_getMotorRPM(mcm));
            first_eff = 0;
        }

        if (!first_eff) printf(",");
        printf("\"time_in_straight\":%.4f", eff->straightTime_s);
        first_eff = 0;

        if (!first_eff) printf(",");
        printf("\"time_in_corners\":%.4f", eff->cornerTime_s);

        if (!first_eff) printf(",");
        printf("\"lap_counter\":%d", eff->lapCounter);

        if (pl != NULL) {
            if (!first_eff) printf(",");
            printf("\"pl_target\":%.4f", (float4)pl->plTargetPower);
        }

        if (!first_eff) printf(",");
        printf("\"energy_consumption_per_lap\":%.4f", eff->lapEnergy_kWh);

        if (!first_eff) printf(",");
        printf("\"energy_budget_per_lap\":%.4f", eff->energyBudget_kWh);

        if (!first_eff) printf(",");
        printf("\"carry_over_energy\":%.4f", eff->carryOverEnergy_kWh);

        if (!first_eff) printf(",");
        printf("\"total_lap_distance\":%.4f", eff->lapDistance_km);

        printf("}");
        first_field = 0;
    }

    if (pl != NULL) {
        if (!first_field) printf(",");
        printf("\"power_limit\":{");
        int first_pl = 1;
        
        if (!first_pl) printf(",");
        printf("\"pl_status\":%s", pl->plStatus ? "true" : "false");
        first_pl = 0;
        
        if (!first_pl) printf(",");
        printf("\"pl_mode\":%u", pl->plMode);
        
        if (!first_pl) printf(",");
        printf("\"pl_target_power\":%.4f", (float4)pl->plTargetPower);
        
        if (!first_pl) printf(",");
        printf("\"pl_initialization_threshold\":%.4f", (float4)pl->plInitializationThreshold);
        
        if (!first_pl) printf(",");
        printf("\"pl_torque_command\":%.4f", (float4)pl->plTorqueCommand / 10.0f);  // Convert DNm to Nm
        
        if (!first_pl) printf(",");
        printf("\"pl_clamping_method\":%u", pl->clampingMethod);
        
        if (!first_pl) printf(",");
        printf("\"pl_always_on\":%s", pl->plAlwaysOn ? "true" : "false");
        
        // Only output current_power_kw if mcm is available
        if (mcm != NULL) {
            sbyte4 current_power_kw = (MCM_getDCVoltage(mcm) * MCM_getDCCurrent(mcm)) / 1000;
            if (!first_pl) printf(",");
            printf("\"current_power_kw\":%.4f", (float4)current_power_kw);
        }
        
        printf("}");
        first_field = 0;
    }

    if (mcm != NULL) {
        if (!first_field) printf(",");
        printf("\"motor_controller\":{");
        int first_mcm = 1;
        
        printf("\"motor_rpm\":%.4f", (float4)MCM_getMotorRPM(mcm));
        first_mcm = 0;
        
        if (!first_mcm) printf(",");
        printf("\"dc_voltage\":%.4f", (float4)MCM_getDCVoltage(mcm));
        
        if (!first_mcm) printf(",");
        printf("\"dc_current\":%.4f", (float4)MCM_getDCCurrent(mcm));
        
        if (!first_mcm) printf(",");
        sbyte4 power_kw = (MCM_getDCVoltage(mcm) * MCM_getDCCurrent(mcm)) / 1000;
        printf("\"power_kw\":%.4f", (float4)power_kw);
        
        if (!first_mcm) printf(",");
        printf("\"commanded_torque\":%.4f", (float4)MCM_getCommandedTorque(mcm) / 10.0f);  // Convert DNm to Nm
        
        if (!first_mcm) printf(",");
        printf("\"apps_torque\":%.4f", (float4)MCM_commands_getAppsTorque(mcm) / 10.0f);  // Convert DNm to Nm
        
        if (!first_mcm) printf(",");
        printf("\"pl_torque_command\":%.4f", (float4)MCM_get_PL_torqueCommand(mcm) / 10.0f);  // Convert DNm to Nm
        
        if (!first_mcm) printf(",");
        printf("\"max_torque\":%.4f", (float4)MCM_getMaxTorqueDNm(mcm) / 10.0f);  // Convert DNm to Nm
        
        if (!first_mcm) printf(",");
        printf("\"ground_speed_kph\":%.4f", MCM_getGroundSpeedKPH(mcm));
        
        printf("}");
        first_field = 0;
    }
    
    printf("}\n");
    fflush(stdout);
}*/

void sil_restore_tps_values(TorqueEncoder* tps, float4* saved_travelPercent, 
                            float4* saved_tps0_percent, float4* saved_tps1_percent)
{
    if (tps != NULL && saved_travelPercent != NULL && 
        *saved_travelPercent != 0.0f) {
        tps->travelPercent = *saved_travelPercent;
        if (saved_tps0_percent != NULL) {
            tps->tps0_percent = *saved_tps0_percent;
        }
        if (saved_tps1_percent != NULL) {
            tps->tps1_percent = *saved_tps1_percent;
        }
    }
}

#endif // SIL_BUILD

