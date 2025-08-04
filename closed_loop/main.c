// main.c - Simple loop to test Motor Controller
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "motorcontroller.h"

// Type definitions
typedef uint8_t ubyte1;
typedef uint16_t ubyte2;
typedef uint32_t ubyte4;
typedef int8_t sbyte1;
typedef int16_t sbyte2;
typedef int32_t sbyte4;
typedef float float4;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

// Global mock sensors
Sensor Sensor_RTDButton = {TRUE, true, "RTDButton"};
Sensor Sensor_HVILTerminationSense = {TRUE, true, "HVILTermSense"};
void* Light_dashRTD = NULL;

// Mock time
static ubyte4 mock_time_us = 0;
#define IO_DO_00 0

// Mock functions (minimal implementation)
void IO_RTC_StartTime(ubyte4* timestamp) { *timestamp = mock_time_us; }
ubyte4 IO_RTC_GetTimeUS(ubyte4 start_timestamp) { return mock_time_us - start_timestamp; }
void IO_DO_Set(ubyte1 pin, bool state) { }
void SerialManager_send(SerialManager* sm, const char* message) { }
void TorqueEncoder_getOutputPercent(TorqueEncoder* te, float4* outputPercent) { *outputPercent = te->outputPercent; }
void RTDS_setVolume(ReadyToDriveSound* rtds, float4 volume, ubyte4 duration) { }
void Light_set(void* light, float4 brightness) { }

int main() {
    // Create objects
    SerialManager mockSerial = {true, ""};
    TorqueEncoder mockTPS = {0.0, true, 0.0};
    BrakePressureSensor mockBPS = {0.0, true};
    ReadyToDriveSound mockRTDS = {0.0, 0, false};
    
    // Create Motor Controller
    MotorController* mcm = MotorController_new(&mockSerial, 0x0A0, FORWARD, 2000);
    
   
    if (!mcm) {
        printf("ERROR: Failed to create Motor Controller\n");
        return -1;
    }
    
    printf("Motor Controller Test Loop\n");
    printf("===========================\n");
    
    // Simple test loop
    for (int i = 0; i < 10; i++) {
        printf("\nLoop %d\n", i+1);
        
        // Set throttle position (0% to 100% over the loop)
        float4 throttle = (float4)i / 9.0; // 0.0 to 1.0
        mockTPS.outputPercent = throttle;
        mockTPS.travelPercent = throttle;
        
        // Calculate commands
        MCM_calculateCommands(mcm, &mockTPS, &mockBPS);
        
        // Print data
        printf("Throttle: %.1f%%\n", throttle * 100);
        printf("Commanded Torque: %d deciNm (%.1f Nm)\n", 
               MCM_commands_getTorque(mcm), MCM_commands_getTorque(mcm) / 10.0);
        printf("Max Torque: %d deciNm (%.1f Nm)\n", 
               MCM_getTorqueMax(mcm), MCM_getTorqueMax(mcm) / 10.0);
        printf("Motor Temperature: %dF\n", MCM_getMotorTemp(mcm));
        
        // Advance time
        mock_time_us += 100000; // 100ms
    }
    
    // Clean up
    free(mcm);
    printf("\nTest complete.\n");
    
    return 0;
}