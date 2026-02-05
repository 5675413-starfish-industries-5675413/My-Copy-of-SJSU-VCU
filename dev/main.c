/*****************************************************************************
* SRE-2 Vehicle Control Firmware for the TTTech HY-TTC 50 Controller (VCU)
******************************************************************************
* For project info and history, see https://github.com/spartanracingelectric/SRE-2
* For software/development questions, email rusty@pedrosatech.com
******************************************************************************
* Files
* The Git repository does not contain the complete firmware for SRE-2.  Modules
* provided by TTTech can be found on the CD that accompanied the VCU. These 
* files can be identified by our naming convetion: TTTech files start with a
* prefix in all caps (such as IO_Driver.h), except for ptypes_xe167.h which
* they also provided.
* For instructions on setting up a build environment, see the SRE-2 getting-
* started document, Programming for the HY-TTC 50, at http://1drv.ms/1NQUppu
******************************************************************************
* Organization
* Our code is laid out in the following manner:
* 
*****************************************************************************/

//-------------------------------------------------------------------
//VCU Initialization Stuff
//-------------------------------------------------------------------

//VCU/C headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef SIL_BUILD
#include <sys/select.h>
#include <unistd.h>
#endif
#include "APDB.h"
#include "IO_DIO.h"
#include "IO_Driver.h" //Includes datatypes, constants, etc - should be included in every c file
#include "IO_RTC.h"
#include "IO_UART.h"
//#include "IO_CAN.h"
//#include "IO_PWM.h"

//Our code
#include "initializations.h"
#include "sensors.h"
#include "canManager.h"
#include "motorController.h"
#include "instrumentCluster.h"
#include "readyToDriveSound.h"
#include "torqueEncoder.h"
#include "brakePressureSensor.h"
#include "wheelSpeeds.h"
#include "safety.h"
#include "sensorCalculations.h"
#include "serial.h"
#include "cooling.h"
#include "bms.h"
#include "LaunchControl.h"
#include "regen.h"
#include "drs.h"
#include "powerLimit.h"
#include "PID.h"
#include "efficiency.h"

// SIL (Software-In-the-Loop) includes
#ifdef SIL_BUILD
#include "parse_values.h"
#endif

// HIL includes
#include "hilParameter.h"

//Application Database, needed for TTC-Downloader
APDB appl_db =
    {
        0 /* ubyte4 versionAPDB        */
        ,
        {0} /* BL_T_DATE flashDate       */
            /* BL_T_DATE buildDate                   */
        ,
        {(ubyte4)(((((ubyte4)RTS_TTC_FLASH_DATE_YEAR) & 0x0FFF) << 0) |
                  ((((ubyte4)RTS_TTC_FLASH_DATE_MONTH) & 0x0F) << 12) |
                  ((((ubyte4)RTS_TTC_FLASH_DATE_DAY) & 0x1F) << 16) |
                  ((((ubyte4)RTS_TTC_FLASH_DATE_HOUR) & 0x1F) << 21) |
                  ((((ubyte4)RTS_TTC_FLASH_DATE_MINUTE) & 0x3F) << 26))},
        0 /* ubyte4 nodeType           */
        ,
        0 /* ubyte4 startAddress       */
        ,
        0 /* ubyte4 codeSize           */
        ,
        0 /* ubyte4 legacyAppCRC       */
        ,
        0 /* ubyte4 appCRC             */
        ,
        1 /* ubyte1 nodeNr             */
        ,
        0 /* ubyte4 CRCInit            */
        ,
        0 /* ubyte4 flags              */
        ,
        0 /* ubyte4 hook1              */
        ,
        0 /* ubyte4 hook2              */
        ,
        0 /* ubyte4 hook3              */
        ,
        APPL_START /* ubyte4 mainAddress        */
        ,
        {0, 1} /* BL_T_CAN_ID canDownloadID */
        ,
        {0, 2} /* BL_T_CAN_ID canUploadID   */
        ,
        0 /* ubyte4 legacyHeaderCRC    */
        ,
        0 /* ubyte4 version            */
        ,
        500 /* ubyte2 canBaudrate        */
        ,
        0 /* ubyte1 canChannel         */
        ,
        {0} /* ubyte1 reserved[8*4]      */
        ,
        0 /* ubyte4 headerCRC          */
};

extern Sensor Sensor_TPS0;
extern Sensor Sensor_TPS1;
extern Sensor Sensor_BPS0;
extern Sensor Sensor_BPS1;
extern Sensor Sensor_WSS_FL;
extern Sensor Sensor_WSS_FR;
extern Sensor Sensor_WSS_RL;
extern Sensor Sensor_WSS_RR;
extern Sensor Sensor_WPS_FL;
extern Sensor Sensor_WPS_FR;
extern Sensor Sensor_WPS_RL;
extern Sensor Sensor_WPS_RR;
extern Sensor Sensor_SAS;
extern Sensor Sensor_TCSKnob;

extern Sensor Sensor_RTDButton;
extern Sensor Sensor_TestButton;
extern Sensor Sensor_TEMP_BrakingSwitch;
extern Sensor Sensor_EcoButton;
extern Sensor Sensor_DRSButton;

/*****************************************************************************
* Main!
* Initializes I/O
* Contains sensor polling loop (always running)
****************************************************************************/
void main(void)
{
    // Test print to verify output is working
    fprintf(stderr, "=== MAIN FUNCTION STARTED ===\n");
    fflush(stderr);
    
    ubyte4 timestamp_startTime = 0;
    ubyte4 timestamp_EcoButton = 0;
    ubyte1 calibrationErrors; //NOT USED


    /*******************************************/
    /*        Low Level Initializations        */
    /*******************************************/
    IO_Driver_Init(NULL); //Handles basic startup for all VCU subsystems

    //Initialize serial first so we can use it to debug init of other subsystems
    SerialManager *serialMan = SerialManager_new();
    IO_RTC_StartTime(&timestamp_startTime);
    SerialManager_send(serialMan, "\n\n\n\n\n\n\n\n\n\n----------------------------------------------------\n");
    SerialManager_send(serialMan, "VCU serial is online.\n");

    //Read initial values from EEPROM
    //EEPROMManager* EEPROMManager_new();

    /*******************************************/
    /*      System Level Initializations       */
    /*******************************************/

    //----------------------------------------------------------------------------
    // Check if we're on the bench or not
    //----------------------------------------------------------------------------
    bool bench;
    IO_DI_Init(IO_DI_06, IO_DI_PD_10K);
    IO_RTC_StartTime(&timestamp_startTime);
    while (IO_RTC_GetTimeUS(timestamp_startTime) < 55555)
    {
        IO_Driver_TaskBegin();

        //IO_DI (digital inputs) supposed to take 2 cycles before they return valid data
        IO_DI_Get(IO_DI_06, &bench);

        IO_Driver_TaskEnd();
        //TODO: Find out if EACH pin needs 2 cycles or just the entire DIO unit
        while (IO_RTC_GetTimeUS(timestamp_startTime) < 10000)
            ; // wait until 10ms have passed
    }
    IO_DI_DeInit(IO_DI_06);
    SerialManager_send(serialMan, bench == TRUE ? "VCU is in bench mode.\n" : "VCU is NOT in bench mode.\n");
    

    //----------------------------------------------------------------------------
    // VCU Subsystem Initializations
    // Eventually, all of these functions should be made obsolete by creating
    // objects instead, like the RTDS/MCM/TPS objects below
    //----------------------------------------------------------------------------
    SerialManager_send(serialMan, "VCU objects/subsystems initializing.\n");
    vcu_initializeADC(bench); //Configure and activate all I/O pins on the VCU
    //vcu_initializeCAN();
    //vcu_initializeMCU();

    //Do some loops until the ADC stops outputting garbage values
    vcu_ADCWasteLoop();

    //vcu_init functions may have to be performed BEFORE creating CAN Manager object
    CanManager *canMan = CanManager_new(500, 50, 50, 500, 10, 10, 200000, serialMan); //3rd param = messages per node (can0/can1; read/write)

    //can0_busSpeed ---------------------^    ^   ^   ^    ^   ^     ^         ^
    //can0_read_messageLimit -----------------|   |   |    |   |     |         |
    //can0_write_messageLimit---------------------+   |    |   |     |         |
    //can1_busSpeed-----------------------------------+    |   |     |         |
    //can1_read_messageLimit-------------------------------+   |     |         |
    //can1_write_messageLimit----------------------------------+     |         |
    //defaultSendDelayus---------------------------------------------+         |
    //SerialManager* sm--------------------------------------------------------+

    //----------------------------------------------------------------------------
    // Object representations of external devices
    // Most default values for things should be specified here
    //----------------------------------------------------------------------------
    // ! to remove -- retired functionality
    // ubyte1 pot_DRS_LC = 0; // 0 is for DRS and 1 is for launch control/Auto DRS - CHANGE HERE FOR POT MODE

    ReadyToDriveSound *rtds = RTDS_new();
    BatteryManagementSystem *bms = BMS_new(serialMan, BMS_BASE_ADDRESS);
    MotorController *mcm0 = MotorController_new(serialMan, 0xA0, FORWARD, 2310, ENDURANCE); //CAN addr, direction, torque limit x10 (100 = 10Nm)
    InstrumentCluster *ic0 = InstrumentCluster_new(serialMan, 0x702);
    TorqueEncoder *tps = TorqueEncoder_new(bench);
    BrakePressureSensor *bps = BrakePressureSensor_new();
    WheelSpeeds *wss = WheelSpeeds_new(WHEEL_DIAMETER, WHEEL_DIAMETER, NUM_BUMPS, NUM_BUMPS);
    SafetyChecker *sc = SafetyChecker_new(serialMan, 320, 32); //Must match amp limits
    CoolingSystem *cs = CoolingSystem_new(serialMan);
    Regen *regen = Regen_new(TRUE);
    LaunchControl *lc = LaunchControl_new(FALSE);
    DRS *drs = DRS_new();
    PowerLimit *pl = POWERLIMIT_new(TRUE);
    Efficiency *eff = EFFICIENCY_new(TRUE);  // Enable efficiency calculations
//---------------------------------------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    // TODO: Additional Initial Power-up functions
    // //----------------------------------------------------------------------------
    // ubyte2 tps0_calibMin = 0xABCD;  //me->tps0->sensorValue;
    // ubyte2 tps0_calibMax = 0x9876;  //me->tps0->sensorValue;
    // ubyte2 tps1_calibMin = 0x5432;  //me->tps1->sensorValue;
    // ubyte2 tps1_calibMax = 0xCDEF;  //me->tps1->sensorValue;
    ubyte2 tps0_calibMin = 700;  //me->tps0->sensorValue;
    ubyte2 tps0_calibMax = 2000; //me->tps0->sensorValue;
    ubyte2 tps1_calibMin = 2600; //me->tps1->sensorValue;
    ubyte2 tps1_calibMax = 5000; //me->tps1->sensorValue;
    //TODO: Read calibration data from EEPROM?
    //TODO: Run calibration functions?
    //TODO: Power-on error checking?

    /*******************************************/
    /*           SIL CONFIGURATION             */
    /*******************************************/
    
    #ifdef SIL_BUILD
    // Static variables to store JSON-parsed TPS values (preserved across loop iterations)
    static float4 saved_tps_travelPercent = 0.0f;
    static float4 saved_tps0_percent = 0.0f;
    static float4 saved_tps1_percent = 0.0f;
    

    {
        // Use a large buffer for JSON (up to 256KB)
        #define JSON_BUFFER_SIZE (256 * 1024)
        char* json_buffer = (char*)malloc(JSON_BUFFER_SIZE);
        if (json_buffer == NULL) {
            fprintf(stderr, "SIL: Failed to allocate JSON buffer\n");
            fflush(stderr);
        } else {
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
            
            if (total_read > 0) {
                int parse_result =
                    parse_struct_values_from_string(json_buffer, pl, mcm0, tps);
        
                if (parse_result != 0) {
                    fprintf(stderr,
                            "SIL: Failed to parse JSON from stdin (error: %d, length: %zu)\n",
                            parse_result, total_read);
                    fprintf(stderr, "SIL: Received JSON (first 200 chars): %.200s\n", json_buffer);
                    if (total_read > 200) {
                        fprintf(stderr, "SIL: Received JSON (last 200 chars): %.200s\n", json_buffer + total_read - 200);
                    }
                    fflush(stderr);
                } else {
                    saved_tps_travelPercent = tps->travelPercent;
                    saved_tps0_percent = tps->tps0_percent;
                    saved_tps1_percent = tps->tps1_percent;
                }
            } else {
                fprintf(stderr, "SIL: No JSON data read from stdin\n");
                fflush(stderr);
            }
            
            free(json_buffer);
        }
    }
    #endif

    /*******************************************/
    /*           HIL CONFIGURATION             */
    /*******************************************/
    HIL_initParamTable(mcm0, pl, lc, wss, bms, regen, eff);

    /*******************************************/
    /*       PERIODIC APPLICATION CODE         */
    /*******************************************/
    /* main loop, executed periodically with a defined cycle time (here: 5 ms) */
    ubyte4 timestamp_mainLoopStart = 0;
    ubyte4 coolingOnTimer = 0;
    ubyte1 coolingOn = 0;
    ubyte4 time = 0;
        //IO_RTC_StartTime(&timestamp_calibStart);
    SerialManager_send(serialMan, "VCU initializations complete.  Entering main loop.\n");
    while (1)
    {   
        //----------------------------------------------------------------------------
        // Task management stuff (start)
        //----------------------------------------------------------------------------
        //Get a timestamp of when this task started from the Real Time Clock
        IO_RTC_StartTime(&timestamp_mainLoopStart);
        //Mark the beginning of a task - what does this actually do?
        IO_Driver_TaskBegin();

        //SerialManager_send(serialMan, "VCU has entered main loop.");

        /*******************************************/
        /*              Read Inputs                */
        /*******************************************/
        //----------------------------------------------------------------------------
        // Handle data input streams
        //----------------------------------------------------------------------------
        //Get readings from our sensors and other local devices (buttons, 12v battery, etc)
        sensors_updateSensors();

        //Pull messages from CAN FIFO and update our object representations.
        CanManager_read(canMan, CAN0_HIPRI, mcm0, ic0, bms, sc, wss);

        // if (Sensor_TestButton.sensorValue == TRUE ) {
        //     // TODO rewire Sensor_TestButton 
        //     lc->buttonDebug |= 0x02;
        // }
        // else {
        //     lc->buttonDebug &= ~0x02;
        // }
        // if (Sensor_DRSButton.sensorValue == TRUE ) { // mark gives 02
        //     lc->buttonDebug |= 0x01;
        // }
        // else {
        //     lc->buttonDebug &= ~0x01;
        // }
        // if (Sensor_EcoButton.sensorValue == TRUE ) { // cal gives 04
        //    lc->buttonDebug |= 0x04;
        // }
        // else {
        //     lc->buttonDebug &= ~0x04;
        // }
        // if (Sensor_LCButton.sensorValue == TRUE) { //drs gives 08
        //   lc->buttonDebug |= 0x08;
        // }
        // else {
        //     lc->buttonDebug &= ~0x08;
        // }

        /*switch (CanManager_getReadStatus(canMan, CAN0_HIPRI))
        {
            case IO_E_OK: SerialManager_send(serialMan, "IO_E_OK: everything fine\n"); break;
            case IO_E_NULL_POINTER: SerialManager_send(serialMan, "IO_E_NULL_POINTER: null pointer has been passed to function\n"); break;
            case IO_E_CAN_FIFO_FULL: SerialManager_send(serialMan, "IO_E_CAN_FIFO_FULL: overflow of FIFO buffer\n"); break;
            case IO_E_CAN_WRONG_HANDLE: SerialManager_send(serialMan, "IO_E_CAN_WRONG_HANDLE: invalid handle has been passed\n"); break;
            case IO_E_CHANNEL_NOT_CONFIGURED: SerialManager_send(serialMan, "IO_E_CHANNEL_NOT_CONFIGURED: the given handle has not been configured\n"); break;
            case IO_E_CAN_OLD_DATA: SerialManager_send(serialMan, "IO_E_CAN_OLD_DATA: no data has been received\n"); break;
            default: SerialManager_send(serialMan, "Warning: Unknown CAN read status\n"); break;
        }*/

        /*******************************************/
        /*          Perform Calculations           */
        /*******************************************/
        //calculations - Now that we have local sensor data and external data from CAN, we can
        //do actual processing work, from pedal travel calcs to traction control
        //calculations_calculateStuff();

        //Run calibration if commanded
        //if (IO_RTC_GetTimeUS(timestamp_calibStart) < (ubyte4)5000000)

        //SensorValue TRUE and FALSE are reversed due to Pull Up Resistor

        if (Sensor_EcoButton.sensorValue == TRUE || (Sensor_RTDButton.sensorValue == FALSE && Sensor_HVILTerminationSense.sensorValue == FALSE) ) // temp make rtd button rtd button in lv
        {
            if (timestamp_EcoButton == 0)
            {
                SerialManager_send(serialMan, "Eco button detected\n");
                IO_RTC_StartTime(&timestamp_EcoButton);
            }
            else if (IO_RTC_GetTimeUS(timestamp_EcoButton) >= 3000000)
            {
                SerialManager_send(serialMan, "Eco button held 3s - starting calibrations\n");
                //calibrateTPS(TRUE, 5);
                if(time==0)
                {
                    IO_RTC_StartTime(&time);
                    IO_DO_Set(IO_DO_02, TRUE);
                    IO_DO_Set(IO_DO_03, TRUE);
                    time++;
                    DRS_open(drs);
                }

                TorqueEncoder_startCalibration(tps, 5);
                BrakePressureSensor_startCalibration(bps, 5);
                Light_set(Light_dashEco, 1);
                //DIGITAL OUTPUT 4 for STATUS LED
            }
        }
        else
        {
            if (IO_RTC_GetTimeUS(timestamp_EcoButton) > 10000 && IO_RTC_GetTimeUS(timestamp_EcoButton) < 1000000)
            {
                SerialManager_send(serialMan, "Eco mode requested\n");
            }
            timestamp_EcoButton = 0;
        }

        if(Sensor_HVILTerminationSense.sensorValue == FALSE && IO_RTC_GetTimeUS(time) > 4000000)
            {
                IO_DO_Set(IO_DO_02, FALSE);
                IO_DO_Set(IO_DO_03, FALSE);
                DRS_close(drs);
            }

        else if(Sensor_HVILTerminationSense.sensorValue == TRUE){
            IO_DO_Set(IO_DO_02, TRUE);
            IO_DO_Set(IO_DO_03, TRUE);
        }
        TorqueEncoder_update(tps);

        // In SIL mode, restore the JSON-parsed TPS values that were overwritten by TorqueEncoder_update
        #ifdef SIL_BUILD
        if (saved_tps_travelPercent != 0.0f) {
            tps->travelPercent = saved_tps_travelPercent;
            tps->tps0_percent = saved_tps0_percent;
            tps->tps1_percent = saved_tps1_percent;
        }
        #endif 
    
        //Every cycle: if the calibration was started and hasn't finished, check the values again
        TorqueEncoder_calibrationCycle(tps, &calibrationErrors); //Todo: deal with calibration errors
        BrakePressureSensor_update(bps, bench);
        BrakePressureSensor_calibrationCycle(bps, &calibrationErrors);

        //TractionControl_update(tps, mcm0, wss, daq);

        //Update WheelSpeed and interpolate
        WheelSpeeds_update(wss, TRUE);

        //Cool DRS things
        DRS_update(drs, mcm0, tps, bps);

        //DataAquisition_update(); //includes accelerometer
        //TireModel_update()
        //ControlLaw_update();
        /*
        ControlLaw //Tq command
            TireModel //used by control law -> read from WSS, accelerometer
            StateObserver //choose driver command or ctrl law
        */

        // CoolingSystem_calculations(cs, MCM_getTemp(mcm0), MCM_getMotorTemp(mcm0), BMS_getHighestCellTemp_degC(bms), &Sensor_HVILTerminationSense);
        // CoolingSystem_enactCooling(cs); //This belongs under outputs but it doesn't really matter for cooling

        //New Code: Pump, ALWAYS ON

        /*
        if (coolingOnTimer == 0) {
            if (Sensor_LCButton.sensorValue == TRUE && Sensor_HVILTerminationSense.sensorValue == FALSE) {
                IO_RTC_StartTime(&coolingOnTimer);
                coolingOn = ~coolingOn;
            }    
        }  
        else {
            if (IO_RTC_GetTimeUS(coolingOnTimer) > 1000000) {
                coolingOnTimer = 0;
            }
        }

        
        if (Sensor_HVILTerminationSense.sensorValue == FALSE) {
            if (coolingOn == 0) {
                IO_DO_Set(IO_DO_02, FALSE);
                IO_DO_Set(IO_DO_03, FALSE);
            }
            else {
                IO_DO_Set(IO_DO_02, TRUE);
                IO_DO_Set(IO_DO_03, TRUE);
            }
        }
        else {
            // cooling on in hv
           IO_DO_Set(IO_DO_02, TRUE);
           IO_DO_Set(IO_DO_03, TRUE);
        }
        */

        //Assign motor controls to MCM command message
        //motorController_setCommands(rtds);
        //DOES NOT set inverter command or rtds flag
        //if (mcm0->regen_mode != REGENMODE_OFF)
       // MCM_setRegenMode(mcm0, REGENMODE_FORMULAE); // TODO: Read regen mode from DCU CAN message - Issue #96
        // MCM_readTCSSettings(mcm0, &Sensor_TCSSwitchUp, &Sensor_TCSSwitchDown, &Sensor_TCSKnob);
        //---------------------------------------------------------------------------------------------------------
        // input the power limit calculation here from mcm 
        //---------------------------------------------------------------------------------------------------------
        // PLMETHOD 1:TQequation+TQPID
         // PLMETHOD 2:TQequation+PWRPID
          // PLMETHOD 3: LUT+TQPID

          //updatepowerforclosedlooptest
          //same thing for rpm
          
          // Increment MCM values for testing
        // Comment out for regular case testing:
        // MCM_incrementVoltageForTesting(mcm0, 5);      // 5V increments
        // MCM_incrementCurrentForTesting(mcm0, 5);      // 5A increments  
        // MCM_incrementRPMForTesting(mcm0, 100);        // 100 RPM increments
        LaunchControl_calculateCommands(lc, tps, bps, mcm0, wss);
        Regen_calculateCommands(regen, mcm0,tps, bps);
        // PowerLimit_updatePLPower(pl);
        PowerLimit_calculateCommands(pl, mcm0, tps);
        Efficiency_calculateCommands(eff, mcm0, pl);
        MCM_calculateCommands(mcm0, tps, bps);
        
        SafetyChecker_update(sc, mcm0, bms, tps, bps, &Sensor_HVILTerminationSense, &Sensor_LVBattery);

        /*******************************************/
        /*  Output Adjustments by Safety Checker   */
        /*******************************************/
        SafetyChecker_reduceTorque(sc, mcm0, bms, wss);

        /*******************************************/
        /*              Enact Outputs              */
        /*******************************************/
        //MOVE INTO SAFETYCHECKER
        //SafetyChecker_setErrorLight(sc);
        Light_set(Light_dashError, (SafetyChecker_getFaults(sc) == 0) ? 0 : 1);
        //Handle motor controller startup procedures
        MCM_relayControl(mcm0, &Sensor_HVILTerminationSense);
        MCM_inverterControl(mcm0, tps, bps, rtds);

        IO_ErrorType err = 0;
        //Comment out to disable shutdown board control
        err = BMS_relayControl(bms);

        //CanManager_sendMCMCommandMessage(mcm0, canMan, FALSE);

        //Drop the sensor readings into CAN (just raw data, not calculated stuff)
        //canOutput_sendMCUControl(mcm0, FALSE);

        //Send debug data
        // Skip CAN debug messages in SIL mode to avoid hangs
        #ifndef SIL_BUILD
        canOutput_sendDebugMessage(canMan, tps, bps, mcm0, ic0, bms, wss, sc, lc, pl, drs, regen, eff);
        canOutput_sendDebugMessage1(canMan, mcm0, tps);
        #endif
        //canOutput_sendSensorMessages();
        //canOutput_sendStatusMessages(mcm0);

        //----------------------------------------------------------------------------
        // Task management stuff (end)
        //----------------------------------------------------------------------------
        RTDS_shutdownHelper(rtds); //Stops the RTDS from playing if the set time has elapsed

        //Task end function for IO Driver - This function needs to be called at the end of every SW cycle
        IO_Driver_TaskEnd();
        
        //wait until the cycle time is over
        while (IO_RTC_GetTimeUS(timestamp_mainLoopStart) < 10000) // 1000 = 1ms
        {
            IO_UART_Task(); //The task function shall be called every SW cycle.
        }

    /*******************************************/
    /*         SIL JSON INPUT (per loop)       */
    /*******************************************/
    #ifdef SIL_BUILD
    // Non-blocking JSON reader for main loop
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
            if (json_buffer != NULL) {
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
                
                if (total_read > 0) {
                    int parse_result = parse_struct_values_from_string(json_buffer, pl, mcm0, tps);
                    if (parse_result != 0) {
                        fprintf(stderr, "SIL: Failed to parse JSON in loop (error: %d)\n", parse_result);
                        fflush(stderr);
                    }
                }
                
                free(json_buffer);
            }
        }
    }
    #endif

    /*******************************************/
    /*              SIL OUTPUTS                */
    /*******************************************/
    #ifdef SIL_BUILD
                // Debug: Check MCM values
                // sbyte4 mcm_voltage = MCM_getDCVoltage(mcm0);
                // sbyte4 mcm_current = MCM_getDCCurrent(mcm0);
                // sbyte4 mcm_rpm = MCM_getMotorRPM(mcm0);
                // sbyte2 mcm_apps_torque = MCM_commands_getAppsTorque(mcm0);
                // sbyte4 mcm_power_kw = (mcm_voltage * mcm_current) / 1000;
                
                // fprintf(stderr, "MCM DC_Voltage: %d V\n", mcm_voltage);
                // fprintf(stderr, "MCM DC_Current: %d A\n", mcm_current);
                // fprintf(stderr, "MCM motorRPM: %d\n", mcm_rpm);
                // ubyte2 mcm_max_torque = MCM_getMaxTorqueDNm(mcm0);
                // fprintf(stderr, "MCM torqueMaximumDNm: %d\n", mcm_max_torque);
                // float4 tps_travel_percent = tps->travelPercent;
                // float4 tps_output_curve = tps->outputCurveExponent;
                // float4 tps_output_percent = 0.0f;
                // TorqueEncoder_getOutputPercent(tps, &tps_output_percent);
                // fprintf(stderr, "TPS travelPercent: %.4f\n", tps_travel_percent);
                // fprintf(stderr, "TPS outputCurveExponent: %.4f\n", tps_output_curve);
                // fprintf(stderr, "TPS outputPercent: %.4f\n", tps_output_percent);
                // fprintf(stderr, "MCM appsTorque: %d (expected: %d)\n", mcm_apps_torque, (sbyte2)(mcm_max_torque * tps_output_percent));
                // fprintf(stderr, "MCM Power (V*I/1000): %d kW\n", mcm_power_kw);
                // fprintf(stderr, "PL plToggle: %s\n", (pl->plToggle ? "True" : "False"));
                // fprintf(stderr, "PL plStatus: %s\n", POWERLIMIT_getStatus(pl) ? "True" : "False");
                // fprintf(stderr, "PL TorqueCommand: %d\n", POWERLIMIT_getTorqueCommand(pl));
                
                // // Calculate power: power = (torque_command/10) * angular_speed * 0.9345
                // // angular_speed = RPM * 2 * PI / 60
                // float angular_speed = (float)mcm_rpm * 2.0f * 3.14159265358979323846f / 60.0f;
                // float power = ((float)POWERLIMIT_getTorqueCommand(pl) / 10.0f) * angular_speed;
                // float electrical_power = power * 0.9345f;  // Convert to electrical power
                // fprintf(stderr, "MCM Power (calculated): %.0fkW\n", electrical_power);
                // fprintf(stderr, "===================\n\n");
                // fflush(stderr);  // Force output to be written immediately

                float4 mcm_motorRPM = MCM_getMotorRPM(mcm0);
                float4 timeinstraight = eff->timeInStraights_s;
                float4 timeincorners = eff->timeInCorners_s;
                float4 lapcounter = eff->lapCounter;
                float4 pltarget = pl->plTargetPower;
                float4 energyconsumptionperlap = eff->lapEnergySpent_kWh;
                float4 energybudgetperlap = eff->energyBudget_kWh;
                float4 carryoverenergy = eff->carryOverEnergy_kWh;
                float4 totallapdistance = eff->totalLapDistance_km;
                float4 lapenergyspent = eff->lapEnergySpent_kWh;

                printf("{\"efficiency\":{"
                    "\"mcm_motorRPM\":%.4f,"
                    "\"time_in_straight\":%.4f,"
                    "\"time_in_corners\":%.4f,"
                    "\"lap_counter\":%.4f,"
                    "\"pl_target\":%.4f,"
                    "\"energy_consumption_per_lap\":%.4f,"
                    "\"energy_budget_per_lap\":%.4f,"
                    "\"carry_over_energy\":%.4f,"
                    "\"total_lap_distance\":%.4f"
                    "}}\n",
                    mcm_motorRPM,
                    timeinstraight,
                    timeincorners,
                    lapcounter,
                    pltarget,
                    energyconsumptionperlap,
                    energybudgetperlap,
                    carryoverenergy,
                    totallapdistance);
             
                fflush(stdout);
        #endif
        
    } //end of main loop

    //----------------------------------------------------------------------------
    // VCU Subsystem Deinitializations
    //----------------------------------------------------------------------------
    //IO_ADC_ChannelDeInit(IO_ADC_5V_00);
    //Free memory if object won't be used anymore
}
