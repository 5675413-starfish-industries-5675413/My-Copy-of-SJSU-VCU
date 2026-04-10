
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

    ubyte1 faultFlags;
    ubyte1 warningFlags;

    ubyte4 hvsensePackVoltage_cV;
    ubyte2 stateOfCharge_mAh;
    
    sbyte2 highestCellVoltage_V;
    sbyte2 lowestCellVoltage_V;
    sbyte4 totalPackVoltage_V;

    sbyte1 highestCellTemperature_C;
    sbyte1 lowestCellTemperature_C;

    bool relayState;
};

BatteryManagementSystem *BMS_new(SerialManager *serialMan, ubyte2 canMessageBaseID)
{
    BatteryManagementSystem *me = (BatteryManagementSystem *)malloc(sizeof(struct _BatteryManagementSystem));

    me->sm = serialMan;

    me->faultFlags = 0;
    me->warningFlags = 0;

    me->hvsensePackVoltage_cV = 0;
    me->totalPackVoltage_V = 0;
    me->stateOfCharge_mAh = 0;

    me->highestCellVoltage_V = 0;
    me->lowestCellVoltage_V = 0;

    me->highestCellTemperature_C = 0;
    me->lowestCellTemperature_C = 0;

    me->relayState = FALSE;

    return me;
}

static ubyte1 parseUbyte1(const ubyte1* data){
    return data[0];
}
static sbyte1 parseSbyte1(const ubyte1* data){
    return (sbyte1)parseUbyte1(data);
}
static ubyte2 parseUbyte2LE(const ubyte1* data){
    return ((ubyte2)data[1] << 8) | ((ubyte2)data[0]);
}
static sbyte2 parseSbyte2LE(const ubyte1* data){
    return (sbyte2)parseUbyte2LE(data);
}

void BMS_parseCanMessage(BatteryManagementSystem *bms, IO_CAN_DATA_FRAME *bmsCanMessage)
{
    ubyte1* data = bmsCanMessage->data;
    switch(bmsCanMessage->id){
        case BMS_PACK_SUMMARY_1:
            bms->highestCellVoltage_V = parseSbyte2LE(&data[0]);
            bms->lowestCellVoltage_V = parseSbyte2LE(&data[2]);
            bms->highestCellTemperature_C = parseSbyte1(&data[4]);
            bms->lowestCellTemperature_C = parseSbyte1(&data[5]);
            bms->totalPackVoltage_V = parseUbyte2LE(&data[6]);
            break;
        case BMS_PACK_SUMMARY_2:
            bms->faultFlags = parseUbyte1(&data[0]);
            bms->warningFlags = parseUbyte1(&data[1]);
            bms->hvsensePackVoltage_cV = parseUbyte2LE(&data[4]);
            bms->stateOfCharge_mAh = parseUbyte2LE(&data[6]);
            break;
        case BMS_HEARTBEAT_MESSAGE:
            break;

    }
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
    return (me->highestCellVoltage_V);
}

ubyte2 BMS_getLowestCellVoltage_mV(BatteryManagementSystem *me)
{
    return (me->lowestCellVoltage_V);
}

ubyte4 BMS_getPackVoltage(BatteryManagementSystem *me)
{
    return (me->hvsensePackVoltage_cV); 
}
//Split into
sbyte2 BMS_getHighestCellTemp_d_degC(BatteryManagementSystem *me)
{
    char buffer[32];
    sprintf(buffer, "highestCellTemp (degC*10): %i\n", (me->highestCellTemperature_C));

    //Need to divide by BMS_TEMPERATURE_SCALE at usage to get deciCelsius value into Celsius
    return (me->highestCellTemperature_C);
}

sbyte2 BMS_getHighestCellTemp_degC(BatteryManagementSystem *me)
{
    char buffer[32];
    sprintf(buffer, "highestCellTemp (degC): %i\n", (me->highestCellTemperature_C/BMS_TEMPERATURE_SCALE));

    //Need to divide by BMS_TEMPERATURE_SCALE at usage to get deciCelsius value into Celsius
    return (me->highestCellTemperature_C/BMS_TEMPERATURE_SCALE);
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