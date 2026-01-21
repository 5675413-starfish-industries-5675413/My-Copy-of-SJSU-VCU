/*****************************************************************************
 * sil.c - Software-In-the-Loop (SIL) implementation
 * Handles JSON input/output for SIL testing mode
 *****************************************************************************/

#include "sil.h"

#ifdef SIL_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

// Include parse_values.h - the build system should handle the include path
#ifdef SIL_BUILD
// Forward declaration - parse_values.h should be available via build system include paths
extern int parse_struct_values_from_string(const char* json_string, 
                                           PowerLimit* pl, 
                                           MotorController* mcm, 
                                           TorqueEncoder* tps);
#endif

int sil_read_initial_json(PowerLimit* pl, MotorController* mcm, TorqueEncoder* tps)
{
    // Use a large buffer for JSON (up to 256KB)
    #define JSON_BUFFER_SIZE (256 * 1024)
    char* json_buffer = (char*)malloc(JSON_BUFFER_SIZE);
    if (json_buffer == NULL) {
        fprintf(stderr, "SIL: Failed to allocate JSON buffer\n");
        fflush(stderr);
        return -1;
    }
    
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
    
    int result = -1;
    if (total_read > 0) {
        result = parse_struct_values_from_string(json_buffer, pl, mcm, tps);
        
        if (result != 0) {
            fprintf(stderr,
                    "SIL: Failed to parse JSON from stdin (error: %d, length: %zu)\n",
                    result, total_read);
            fprintf(stderr, "SIL: Received JSON (first 200 chars): %.200s\n", json_buffer);
            if (total_read > 200) {
                fprintf(stderr, "SIL: Received JSON (last 200 chars): %.200s\n", json_buffer + total_read - 200);
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
    fd_set readfds;
    struct timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(0, &readfds); // stdin is file descriptor 0
    timeout.tv_sec = 0;
    timeout.tv_usec = 0; // Non-blocking check
    
    if (select(1, &readfds, NULL, NULL, &timeout) > 0 && FD_ISSET(0, &readfds)) {
        // Data available, read JSON
        #define JSON_BUFFER_SIZE (256 * 1024)
        char* json_buffer = (char*)malloc(JSON_BUFFER_SIZE);
        if (json_buffer == NULL) {
            return -1;
        }
        
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
    
    // No data available (non-blocking)
    return 1;
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

void sil_write_json_output(MotorController* mcm, PowerLimit* pl, Efficiency* eff)
{
    int first_field = 1;  // Track if this is the first field in JSON
    
    printf("{");
    
    // Output Efficiency values if requested
    if (sil_output_mode & SIL_OUTPUT_EFFICIENCY) {
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
            printf("\"lap_counter\":%.4f", eff->lapCounter);
            
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
    if ((sil_output_mode & SIL_OUTPUT_POWERLIMIT) && pl != NULL) {
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
    if ((sil_output_mode & SIL_OUTPUT_MOTORCTRL) && mcm != NULL) {
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

