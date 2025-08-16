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

    // Control parameters
    sbyte2 torqueMaxInDNM = 2000;
    sbyte1 minRegenSpeedKPH = 5;
    sbyte1 regenRampdownStartSpeed = 10;
    RegenMode regenMode = REGENMODE_FIXED;

    //groundKPH parameters
    sbyte4 motorRPM = 500;
    sbyte4 FD_Ratio = 3.55; //divide # of rear teeth by number of front teeth
    sbyte4 Revolutions = 60; //this converts the rpm to rotations per hour
    //tireCirc does PI * Diameter_Tire because otherwise it doesn't work
    //for 16s set tireCirc to 1.295 for 18s set tireCirc to 1.395 
    //sbyte4 PI = 3.141592653589; 
    //sbyte4 Diameter_Tire = 0.4;
    sbyte4 tireCirc = 1.395; //the actual average tire circumference in meters
    sbyte4 KPH_Unit_Conversion = 1000.0;


    // Create Motor Controller
    MotorController* mcm = MotorController_new(&mockSerial, 0x0A0, FORWARD, torqueMaxInDNM, minRegenSpeedKPH, regenRampdownStartSpeed);

    if (!mcm) {
        printf("ERROR: Failed to create Motor Controller\n");
        return -1;
    }
    
    //No regen below 5kph
    sbyte4 groundSpeedKPH = MCM_getGroundSpeedKPH(mcm, motorRPM, FD_Ratio, Revolutions, tireCirc, KPH_Unit_Conversion);
    if (groundSpeedKPH < 15)
    {
        MCM_setRegenMode(mcm, regenMode);
    } else {
        // Regen mode is now set based on battery voltage to preserve overvoltage fault 
        // if(BMS_getPackVoltage(bms) >= 38500 * 10){ 
        //     MCM_setRegenMode(mcm0, REGENMODE_FORMULAE); 
        // } else {
        //     MCM_setRegenMode(mcm0, REGENMODE_FIXED);
        // } 
        regenMode = REGENMODE_OFF;
        printf("Regen mode set to OFF due to ground speed.\n\n");
    }

    const char* RegenModeNames[] = {
    "REGENMODE_OFF",
    "REGENMODE_FORMULAE",
    "REGENMODE_HYBRID",
    "REGENMODE_TESLA",
    "REGENMODE_FIXED"
    };

    printf("Motor Controller Test Loop\n");
    printf("===========================\n");
    printf("Config\n");
    printf("torqueMaxInDNM  = %d\n", torqueMaxInDNM);
    printf("minRegenSpeedKPH = %d\n", minRegenSpeedKPH);
    printf("regenRampdownStartSpeed = %d\n", regenRampdownStartSpeed);
    printf("regenMode = %s\n", RegenModeNames[regenMode]);
    printf("Ground Speed KPH = %d\n", groundSpeedKPH);
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
        printf("appsTorque: %d (%.1f Nm)\n", MCM_getAppsTorque(mcm), MCM_getAppsTorque(mcm) / 10.0);
        printf("bpsTorque: %d (%.1f Nm)\n", MCM_getBpsTorque(mcm), MCM_getBpsTorque(mcm) / 10.0);

        // Advance time
        mock_time_us += 100000; // 100ms
    }
    
    // Clean up
    free(mcm);
    printf("\nTest complete.\n");
    
    return 0;
}