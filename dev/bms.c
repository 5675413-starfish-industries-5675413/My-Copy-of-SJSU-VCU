
#include <stdio.h>
#include "bms.h"
#include <stdlib.h>
#include "IO_Driver.h"
#include "IO_RTC.h"
#include "IO_DIO.h"
#include "serial.h"
#include "mathFunctions.h"

/*********************************************************
 *            *********** CAUTION ***********            *
 * MULTI-BYTE VALUES FOR THE STAFL BMS ARE LITTLE-ENDIAN *
 *                                                       *
 *********************************************************/

#define BMS_NUM_MODULES 12
#define NUM_THERMS_PER_MODULE 10
#define NUM_CELLS_PER_MODULE 14

struct _BatteryManagementSystem
{
    SerialManager *sm;

    // BMS_MASTER_FAULTS //
     ubyte1 faultFlags;
     
     // BMS_MASTER_WARNINGS //
     ubyte1 warningFlags;

    // BMS_PACK_LEVEL_MEASUREMENTS //
    ubyte4 packVoltage;                         // V*1000
    sbyte4 sumCellVoltages;                     // V*1000,
    ubyte2 stateOfCharge;                       // %*10

    // BMS_CELL_VOLTAGE_SUMMARY //
    ubyte4 highestCellVoltage;                  // V*1000
    ubyte2 lowestCellVoltage;                   // V*1000

    // BMS_CELL_TEMPERATURE_SUMMARY //
    sbyte2 highestCellTemperature;              // degC*10
    sbyte2 lowestCellTemperature;               // degC*10

    bool relayState;
};

BatteryManagementSystem *BMS_new(SerialManager *serialMan, ubyte2 canMessageBaseID)
{
    BatteryManagementSystem *me = (BatteryManagementSystem *)malloc(sizeof(struct _BatteryManagementSystem));

    me->sm = serialMan;

    me->faultFlags = 0;
    me->warningFlags = 0;

    me->packVoltage = 0;
    me->sumCellVoltages = 0;
    me->stateOfCharge = 0;

    me->highestCellVoltage = 0;
    me->lowestCellVoltage = 0;

    me->highestCellTemperature = 0;
    me->lowestCellTemperature = 0;

    me->relayState = FALSE;

    return me;
}

void BMS_parseCanMessage(BatteryManagementSystem *bms, IO_CAN_DATA_FRAME *bmsCanMessage)
{
   return;
}

IO_ErrorType BMS_relayControl(BatteryManagementSystem *me)
{
    //////////////////////////////////////////////////////////////
    // Digital output to drive a signal to the Shutdown signal  //
    // based on AMS fault detection                             //
    //////////////////////////////////////////////////////////////
    IO_ErrorType err;
    //There is a fault
    // if (BMS_getFaultFlags0(me) || BMS_getFaultFlags1(me))
    // {
    //     me->relayState = TRUE;
    //     err = IO_DO_Set(IO_DO_01, TRUE); //Drive BMS relay true (HIGH)
    // }
    // //There is no fault
    // else
    // {
    //     me->relayState = FALSE;
    //     err = IO_DO_Set(IO_DO_01, FALSE); //Drive BMS relay false (LOW)
    // }
    return err;
}

/*
sbyte1 BMS_getAvgTemp(BatteryManagementSystem *me)
{
    char buffer[32];
    sprintf(buffer, "AvgPackTemp: %i\n", me->avgTemp);
    return (me->avgTemp);
}
*/

ubyte4 BMS_getHighestCellVoltage_mV(BatteryManagementSystem *me)
{
    return (me->highestCellVoltage);
}

ubyte2 BMS_getLowestCellVoltage_mV(BatteryManagementSystem *me)
{
    return (me->lowestCellVoltage);
}

ubyte4 BMS_getPackVoltage(BatteryManagementSystem *me)
{
    return (me->packVoltage); 
}
//Split into
sbyte2 BMS_getHighestCellTemp_d_degC(BatteryManagementSystem *me)
{
    char buffer[32];
    sprintf(buffer, "highestCellTemp (degC*10): %i\n", (me->highestCellTemperature));

    //Need to divide by BMS_TEMPERATURE_SCALE at usage to get deciCelsius value into Celsius
    return (me->highestCellTemperature);
}

sbyte2 BMS_getHighestCellTemp_degC(BatteryManagementSystem *me)
{
    char buffer[32];
    sprintf(buffer, "highestCellTemp (degC): %i\n", (me->highestCellTemperature/BMS_TEMPERATURE_SCALE));

    //Need to divide by BMS_TEMPERATURE_SCALE at usage to get deciCelsius value into Celsius
    return (me->highestCellTemperature/BMS_TEMPERATURE_SCALE);
}

// ***NOTE: packCurrent and and packVoltage are SIGNED variables and the return type for BMS_getPower is signed
// sbyte4 BMS_getPower_uW(BatteryManagementSystem *me)
// {
//     //char buffer[32];
//     //sprintf(buffer, "power (uW): %f\n", (me->packCurrent * me->packVoltage));

//     //Need to divide by BMS_POWER_SCALE at usage to get microWatt value into Watts
//     return (me->packCurrent * me->packVoltage);
// }

// // ***NOTE: packCurrent and and packVoltage are SIGNED variables and the return type for BMS_getPower is signed
// sbyte4 BMS_getPower_W(BatteryManagementSystem *me)
// {
//     //char buffer[32];
//     //sprintf(buffer, "power (W): %f\n", ((me->packCurrent * me->packVoltage)/BMS_POWER_SCALE));

//     //Need to divide by BMS_POWER_SCALE at usage to get microWatt value into Watts
//     return ((me->packCurrent * me->packVoltage)/BMS_POWER_SCALE);
// }

// ubyte1 BMS_getFaultFlags0(BatteryManagementSystem *me) {
//     //Flag 0x01: Isolation Leakage Fault
//     //Flag 0x02: BMS Monitor Communication Fault
//     //Flag 0x04: Pre-charge Fault
//     //Flag 0x08: Pack Discharge Operating Envelope Exceeded
//     //Flag 0x10: Pack Charge Operating Envelope Exceeded
//     //Flag 0x20: Failed Thermistor Fault
//     //Flag 0x40: HVIL Fault
//     //Flag 0x80: Emergency Stop Fault
//     return me->faultFlags0;
// }

// ubyte1 BMS_getFaultFlags1(BatteryManagementSystem *me) {
//     //Flag 0x01: Cell Over-Voltage Fault
//     //Flag 0x02: Cell Under-Voltage Fault
//     //Flag 0x04: Cell Over-Temperature Fault
//     //Flag 0x08: Cell Under-Temperature Fault
//     //Flag 0x10: Pack Over-Voltage Fault
//     //Flag 0x20: Pack Under-Voltage Fault
//     //Flag 0x40: Over-Current Discharge Fault
//     //Flag 0x80: Over-Current Charge Fault
//     return me->faultFlags1;
// }

bool BMS_getRelayState(BatteryManagementSystem *me) {
    //Return state of shutdown board relay
    return me->relayState;
}

/*
ubyte2 BMS_getPackTemp(BatteryManagementSystem *me)
{
    char buffer[32];
    sprintf(buffer, "PackTemp: %i\n", me->packTemp);
    return (me->packTemp);
}
*/