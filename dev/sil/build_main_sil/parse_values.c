/*****************************************************************************
 * parse_values.c - Parse JSON configuration values into structs
 * Parses struct_members_output.json and assigns values to struct instances
 * 
 * Note: This implementation uses cJSON library for JSON parsing.
 *****************************************************************************/

#include "parse_values.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

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
    double double_val;
    
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
int parse_PowerLimit_from_json(PowerLimit* pl, void* json_params)
{
    if (pl == NULL || json_params == NULL) {
        return -1;
    }
    
    cJSON* params = (cJSON*)json_params;
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
int parse_MotorController_from_json(MotorController* mcm, void* json_params)
{
    if (mcm == NULL || json_params == NULL) {
        return -1;
    }
    
    cJSON* params = (cJSON*)json_params;
    int int_val;
    
    // Set DC Voltage (increment from initial value of 13)
    // Note: These increment functions are used with absolute values in tests
    if (get_json_int_safe(params, "DC_Voltage", &int_val)) {
        // Initial value is 13, so increment by (target - 13)
        sbyte4 increment = (sbyte4)(int_val - 13);
        MCM_incrementVoltageForTesting(mcm, increment);
    }
    
    // Set DC Current (increment from initial value of 13)
    if (get_json_int_safe(params, "DC_Current", &int_val)) {
        // Initial value is 13, so increment by (target - 13)
        sbyte4 increment = (sbyte4)(int_val - 13);
        MCM_incrementCurrentForTesting(mcm, increment);
    }
    
    // Set Motor RPM (increment from initial value of 0)
    if (get_json_int_safe(params, "motorRPM", &int_val)) {
        // Initial value is 0, so increment by target value
        MCM_incrementRPMForTesting(mcm, (sbyte4)int_val);
    }
    
    // Note: commandedTorque and appsTorque are typically set by MCM_calculateCommands()
    // after TPS values are configured, so we don't set them here
    
    return 0;
}

/**
 * Parse TorqueEncoder struct values from JSON object
 */
int parse_TorqueEncoder_from_json(TorqueEncoder* tps, void* json_params)
{
    if (tps == NULL || json_params == NULL) {
        return -1;
    }
    
    cJSON* params = (cJSON*)json_params;
    int int_val;
    double double_val;
    bool bool_val;
    
    // Note: bench, tps0, tps1 are typically set during initialization
    // We skip pointer and initialization-time values (marked as null in JSON)
    
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
    }
    if (get_json_double_safe(params, "travelPercent", &double_val)) {
        tps->travelPercent = (float4)double_val;
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
 * Main function to parse struct_members_output.json and assign values to structs
 */
int parse_struct_values_from_json(const char* json_file_path, 
                                   PowerLimit* pl, 
                                   MotorController* mcm, 
                                   TorqueEncoder* tps)
{
    // Read JSON file
    FILE* file = fopen(json_file_path, "r");
    if (file == NULL) {
        return -1; // Failed to open file
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate buffer and read file
    char* json_string = (char*)malloc(file_size + 1);
    if (json_string == NULL) {
        fclose(file);
        return -1;
    }
    
    size_t read_size = fread(json_string, 1, file_size, file);
    json_string[read_size] = '\0';
    fclose(file);
    
    // Parse JSON
    cJSON* json = cJSON_Parse(json_string);
    free(json_string);
    
    if (json == NULL) {
        return -1; // Failed to parse JSON
    }
    
    // Check if it's an array (struct_members_output.json contains an array of structs)
    if (!cJSON_IsArray(json)) {
        cJSON_Delete(json);
        return -1;
    }
    
    // Iterate through array to find the structs we need
    int array_size = cJSON_GetArraySize(json);
    for (int i = 0; i < array_size; i++) {
        cJSON* struct_item = cJSON_GetArrayItem(json, i);
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
        }
    }
    
    cJSON_Delete(json);
    return 0;
}

/**
 * Main function to parse JSON string and assign values to structs
 */
int parse_struct_values_from_string(const char* json_string, 
                                     PowerLimit* pl, 
                                     MotorController* mcm, 
                                     TorqueEncoder* tps)
{
    if (json_string == NULL) {
        return -1; // Invalid input
    }
    
    // Parse JSON
    cJSON* json = cJSON_Parse(json_string);
    if (json == NULL) {
        // Get error info if available
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            // Error info available, but we can't return it easily, so just return -1
        }
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
        }
    }
    
    cJSON_Delete(json);
    return 0;
}

