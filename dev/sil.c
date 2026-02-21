/*****************************************************************************
 * sil.c - Software-In-the-Loop (SIL) implementation
 * Handles JSON input/output for SIL testing mode
 *****************************************************************************/

#include "sil.h"

#ifdef SIL_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Platform-specific includes for select()
#ifdef _WIN32
    #include <winsock2.h>
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
 * Dynamic Struct Selection Variables
 * Store requested struct names for runtime output selection
 *****************************************************************************/

// Dynamic struct name storage (replaces bitmask system)
#define MAX_REQUESTED_STRUCTS 16
static char sil_requested_structs[MAX_REQUESTED_STRUCTS][64];
static int sil_requested_count = 0;

// Helper function to check if a struct is requested by name
static int is_struct_requested(const char* struct_name)
{
    if (struct_name == NULL) {
        return 0;
    }
    for (int i = 0; i < sil_requested_count; i++) {
        if (strcmp(sil_requested_structs[i], struct_name) == 0) {
            return 1;
        }
    }
    return 0;
}

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
    
    // Set DC Voltage (increment from initial value of 13)
    if (get_json_int_safe(params, "DC_Voltage", &int_val)) {
        sbyte4 increment = (sbyte4)(int_val - 13);
        MCM_incrementVoltageForTesting(mcm, increment);
    }
    
    // Set DC Current (increment from initial value of 13)
    if (get_json_int_safe(params, "DC_Current", &int_val)) {
        sbyte4 increment = (sbyte4)(int_val - 13);
        MCM_incrementCurrentForTesting(mcm, increment);
    }
    
    // Set Motor RPM (increment from initial value of 0)
    if (get_json_int_safe(params, "motorRPM", &int_val)) {
        MCM_incrementRPMForTesting(mcm, (sbyte4)int_val);
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
 * Parse JSON string and assign values to structs
 * This is the main function used by sil_read_initial_json and sil_read_json_input
 */
static int parse_struct_values_from_string(const char* json_string, 
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
        return -1; // Failed to parse JSON
    }
    
    // Check for output request first
    if (cJSON_IsObject(json)) {
        cJSON* request_outputs = cJSON_GetObjectItem(json, "request_outputs");
        if (request_outputs != NULL && cJSON_IsArray(request_outputs)) {
            // Parse requested struct names dynamically (no hardcoded checks!)
            sil_requested_count = 0;  // Clear previous requests
            int array_size = cJSON_GetArraySize(request_outputs);
            for (int i = 0; i < array_size && sil_requested_count < MAX_REQUESTED_STRUCTS; i++) {
                cJSON* item = cJSON_GetArrayItem(request_outputs, i);
                if (item != NULL && cJSON_IsString(item)) {
                    const char* struct_name = item->valuestring;
                    // Store the struct name (accepts any struct name dynamically)
                    strncpy(sil_requested_structs[sil_requested_count], struct_name, 63);
                    sil_requested_structs[sil_requested_count][63] = '\0';
                    sil_requested_count++;
                }
            }
            // If only output request, return early
            if (cJSON_GetObjectItem(json, "structs") == NULL) {
                cJSON_Delete(json);
                return 0; // Successfully set output mode
            }
        }
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

int sil_read_initial_json(PowerLimit* pl, MotorController* mcm, TorqueEncoder* tps)
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
        result = parse_struct_values_from_string(json_buffer, pl, mcm, tps);
        
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

int sil_read_json_input(PowerLimit* pl, MotorController* mcm, TorqueEncoder* tps)
{
    // Non-blocking check for stdin data
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
    
    // Data available, read JSON
    char* json_buffer = (char*)malloc(JSON_BUFFER_SIZE);
    if (json_buffer == NULL) {
        return -1;
    }
    
    int total_read = read_json_from_stdin(json_buffer);
    int parse_result = -1;
    
    if (total_read > 0) {
        parse_result = parse_struct_values_from_string(json_buffer, pl, mcm, tps);
        if (parse_result != 0) {
            fprintf(stderr, "SIL: Failed to parse JSON in loop (error: %d)\n", parse_result);
            fflush(stderr);
        }
    }
    
    free(json_buffer);
    return parse_result;
}

// Static variable to store output mode (set via compile-time define SIL_OUTPUT_MODE_CONFIG)
static ubyte1 sil_output_mode = SIL_OUTPUT_MODE_CONFIG;

void sil_set_output_mode(ubyte1 mode)
{
    sil_output_mode = mode;
}

ubyte1 sil_get_output_mode(void)
{
    return sil_output_mode;
}

ubyte1 sil_get_requested_output_mode(void)
{
    // Return 0 to indicate dynamic mode is being used
    // This maintains backward compatibility but dynamic lookup takes precedence
    return 0;
}

void sil_write_json_output_cJSON_version(MotorController* mcm, PowerLimit* pl, Efficiency* eff, ubyte1 output_mode) {
    if (sil_requested_count > 0) {
        cJSON *root = cJSON_CreateObject();
        if (root == NULL) {
            return ;
        }

        if (is_struct_requested("MotorController") && mcm != NULL) {
            cJSON *mcm_json = cJSON_CreateObject();
            if (mcm_json != NULL) {
                cJSON_AddNumberToObject(mcm_json, "motorRPM", MCM_getMotorRPM(mcm));
                cJSON_AddNumberToObject(mcm_json, "DC_Voltage", MCM_getDCVoltage(mcm));
                cJSON_AddNumberToObject(mcm_json, "DC_Current", MCM_getDCCurrent(mcm));
                cJSON_AddNumberToObject(mcm_json, "power_kw", (MCM_getDCVoltage(mcm) * MCM_getDCCurrent(mcm)) / 1000);
                cJSON_AddNumberToObject(mcm_json, "commandedTorque", MCM_getCommandedTorque(mcm) / 10.0f);
                cJSON_AddNumberToObject(mcm_json, "appsTorque", MCM_commands_getAppsTorque(mcm) / 10.0f);
                cJSON_AddNumberToObject(mcm_json, "plTorqueCommand", MCM_get_PL_torqueCommand(mcm) / 10.0f);
                cJSON_AddNumberToObject(mcm_json, "maxTorque", MCM_getMaxTorqueDNm(mcm) / 10.0f);
                cJSON_AddNumberToObject(mcm_json, "ground_speed_kph", MCM_getGroundSpeedKPH(mcm));
                cJSON_AddItemToObject(root, "motor_controller", mcm_json);
            }
        }
        if (is_struct_requested("PowerLimit") && pl != NULL) {
            cJSON *pl_json = cJSON_CreateObject();
            if (pl_json != NULL) {
                cJSON_AddBoolToObject(pl_json, "plStatus", pl->plStatus);
                cJSON_AddNumberToObject(pl_json, "plMode", pl->plMode);
                cJSON_AddNumberToObject(pl_json, "plTargetPower", pl->plTargetPower);
                cJSON_AddNumberToObject(pl_json, "plInitializationThreshold", pl->plInitializationThreshold);
                cJSON_AddNumberToObject(pl_json, "plTorqueCommand", pl->plTorqueCommand / 10.0f);
                cJSON_AddNumberToObject(pl_json, "clampingMethod", pl->clampingMethod);
                cJSON_AddBoolToObject(pl_json, "plAlwaysOn", pl->plAlwaysOn);
                if (mcm != NULL) {
                    cJSON_AddNumberToObject(pl_json, "current_power_kw", (MCM_getDCVoltage(mcm) * MCM_getDCCurrent(mcm)) / 1000);
                }
                cJSON_AddItemToObject(root, "power_limit", pl_json);
            }
        }
        if (is_struct_requested("Efficiency") && eff != NULL) {
            cJSON *eff_json = cJSON_CreateObject();
            if (eff_json != NULL) {
                cJSON_AddNumberToObject(eff_json, "timeInStraight_s", eff->timeInStraights_s);
                cJSON_AddNumberToObject(eff_json, "timeInCorners_s", eff->timeInCorners_s);
                cJSON_AddNumberToObject(eff_json, "lapCounter", eff->lapCounter);
                if (pl != NULL) {
                    cJSON_AddNumberToObject(eff_json, "pl_target", pl->plTargetPower);
                }
                cJSON_AddNumberToObject(eff_json, "lapEnergySpent_kWh", eff->lapEnergySpent_kWh);
                cJSON_AddNumberToObject(eff_json, "energyBudget_kWh", eff->energyBudget_kWh);
                cJSON_AddNumberToObject(eff_json, "carryOverEnergy_kWh", eff->carryOverEnergy_kWh);
                cJSON_AddItemToObject(root, "efficiency", eff_json);
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
    
}

void sil_write_json_output(MotorController* mcm, PowerLimit* pl, Efficiency* eff, ubyte1 output_mode)
{
    int first_field = 1;  // Track if this is the first field in JSON
    
    printf("{");
    
    // Use dynamic struct name lookup if structs were requested by name
    if (sil_requested_count > 0) {
        // Check each requested struct dynamically
        if (is_struct_requested("MotorController") && mcm != NULL) {
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
        
        if (is_struct_requested("PowerLimit") && pl != NULL) {
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
        
        if (is_struct_requested("Efficiency") && eff != NULL) {
            if (!first_field) printf(",");
            printf("\"efficiency\":{");
            int first_eff = 1;
            
            if (!first_eff) printf(",");
            printf("\"time_in_straight\":%.4f", eff->timeInStraights_s);
            first_eff = 0;
            
            if (!first_eff) printf(",");
            printf("\"time_in_corners\":%.4f", eff->timeInCorners_s);
            
            if (!first_eff) printf(",");
            printf("\"lap_counter\":%d", eff->lapCounter);
            
            // pl_target is optional - only output if pl is available
            if (pl != NULL) {
                if (!first_eff) printf(",");
                printf("\"pl_target\":%.4f", (float4)pl->plTargetPower);
            }
            
            if (!first_eff) printf(",");
            printf("\"energy_consumption_per_lap\":%.4f", eff->lapEnergySpent_kWh);
            
            if (!first_eff) printf(",");
            printf("\"energy_budget_per_lap\":%.4f", eff->energyBudget_kWh);
            
            if (!first_eff) printf(",");
            printf("\"carry_over_energy\":%.4f", eff->carryOverEnergy_kWh);
            
            if (!first_eff) printf(",");
            printf("\"total_lap_distance\":%.4f", eff->totalLapDistance_km);
            
            printf("}");
            first_field = 0;
        }
        
    } else {
        // Fall back to bitmask mode for backward compatibility
        ubyte1 mode = (output_mode != 0) ? output_mode : sil_output_mode;
        
        // Output Efficiency values if requested
        if (mode & SIL_OUTPUT_EFFICIENCY) {
            if (!first_field) printf(",");
            printf("\"efficiency\":{");
            int first_eff = 1;
            
            // Only output efficiency fields if eff struct is available
            if (eff != NULL) {
            if (mcm != NULL) {
                if (!first_eff) printf(",");
                printf("\"mcm_motorRPM\":%.4f", (float4)MCM_getMotorRPM(mcm));
                first_eff = 0;
            }
            
            if (!first_eff) printf(",");
            printf("\"time_in_straight\":%.4f", eff->timeInStraights_s);
            first_eff = 0;
            
            if (!first_eff) printf(",");
            printf("\"time_in_corners\":%.4f", eff->timeInCorners_s);
            
            if (!first_eff) printf(",");
            printf("\"lap_counter\":%d", eff->lapCounter);
            
            // pl_target is optional - only output if pl is available
            if (pl != NULL) {
                if (!first_eff) printf(",");
                printf("\"pl_target\":%.4f", (float4)pl->plTargetPower);
            }
            
            if (!first_eff) printf(",");
            printf("\"energy_consumption_per_lap\":%.4f", eff->lapEnergySpent_kWh);
            
            if (!first_eff) printf(",");
            printf("\"energy_budget_per_lap\":%.4f", eff->energyBudget_kWh);
            
            if (!first_eff) printf(",");
            printf("\"carry_over_energy\":%.4f", eff->carryOverEnergy_kWh);
            
            if (!first_eff) printf(",");
            printf("\"total_lap_distance\":%.4f", eff->totalLapDistance_km);
        }
        
        printf("}");
        first_field = 0;
    }
    
    // Output Power Limit values if requested
    if ((mode & SIL_OUTPUT_POWERLIMIT) && pl != NULL) {
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
    
    // Output Motor Controller values if requested
    if ((mode & SIL_OUTPUT_MOTORCTRL) && mcm != NULL) {
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
    }
    
    printf("}\n");
    fflush(stdout);
}

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

