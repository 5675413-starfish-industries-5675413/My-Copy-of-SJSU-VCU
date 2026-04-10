
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

    ubyte2 packImbalance_mV;
    ubyte2 hvsensePackVoltage_cV;
    ubyte2 stateOfCharge_mAh;
    
    sbyte2 highestCellVoltage_V;
    sbyte2 lowestCellVoltage_V;
    sbyte2 totalPackVoltage_V;

    sbyte1 highestCellTemperature_C;
    sbyte1 lowestCellTemperature_C;

    ubyte2 heartbeatCount;
    
    bool relayState;
};

BatteryManagementSystem *BMS_new(SerialManager *serialMan)
{
    BatteryManagementSystem *me = (BatteryManagementSystem *)malloc(sizeof(struct _BatteryManagementSystem));

    me->sm = serialMan;

    me->faultFlags = 0;
    me->warningFlags = 0;

    me->packImbalance_mV = 0;
    me->hvsensePackVoltage_cV = 0;
    me->stateOfCharge_mAh = 0;
    
    me->highestCellVoltage_V = 0;
    me->lowestCellVoltage_V = 0;
    me->totalPackVoltage_V = 0;

    me->highestCellTemperature_C = 0;
    me->lowestCellTemperature_C = 0;

    me->heartbeatCount = 0;

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
            bms->packImbalance_mV = parseSbyte2LE(&data[2]);
            bms->hvsensePackVoltage_cV = parseUbyte2LE(&data[4]);
            bms->stateOfCharge_mAh = parseUbyte2LE(&data[6]);
            break;
        case BMS_HEARTBEAT_MESSAGE:
            if(parseUbyte1(&data[0]) == 1)
                bms->heartbeatCount++;
            break;
    }
   return;
}

IO_ErrorType BMS_relayControl(BatteryManagementSystem *bms)
{
    //////////////////////////////////////////////////////////////
    // Digital output to drive a signal to the Shutdown signal  //
    // based on AMS fault detection                             //
    //////////////////////////////////////////////////////////////
    IO_ErrorType err;

    static ubyte4 lastBeatTimeStamp;
    static bool initialized = FALSE;
    static ubyte2 previousHeartbeatCount = 0;

    if(!initialized){
        IO_RTC_StartTime(&lastBeatTimeStamp);
        previousHeartbeatCount = bms->heartbeatCount;
        initialized = TRUE;
    }

    if(previousHeartbeatCount == bms->heartbeatCount && IO_RTC_GetTimeUS(lastBeatTimeStamp) >= 5000000u){
        bms->relayState = TRUE;
        err = IO_DO_Set(IO_DO_01, TRUE); //Drive BMS relay true (HIGH)
    }
    else if(previousHeartbeatCount != bms->heartbeatCount){
        previousHeartbeatCount = bms->heartbeatCount;
        IO_RTC_StartTime(&lastBeatTimeStamp);
        bms->relayState = FALSE;
        err = IO_DO_Set(IO_DO_01, FALSE); //Drive BMS relay false (LOW)
    }
    return err;
}

ubyte1 BMS_getFaultFlags(BatteryManagementSystem *me)
{
    return me->faultFlags;
}

ubyte1 BMS_getWarningFlags(BatteryManagementSystem *me)
{
    return me->warningFlags;
}

ubyte2 BMS_getPackImbalance_mV(BatteryManagementSystem *me){
    return me->packImbalance_mV;
}

ubyte2 BMS_getHvSensePackVoltage_cV(BatteryManagementSystem *me)
{
    return me->hvsensePackVoltage_cV;
}

ubyte2 BMS_getStateOfCharge_mAh(BatteryManagementSystem *me)
{
    return me->stateOfCharge_mAh;
}

sbyte2 BMS_getHighestCellVoltage_V(BatteryManagementSystem *me)
{
    return me->highestCellVoltage_V;
}

sbyte2 BMS_getLowestCellVoltage_V(BatteryManagementSystem *me)
{
    return me->lowestCellVoltage_V;
}

sbyte2 BMS_getTotalPackVoltage_V(BatteryManagementSystem *me)
{
    return me->totalPackVoltage_V;
}

sbyte1 BMS_getHighestCellTemperature_C(BatteryManagementSystem *me)
{
    return me->highestCellTemperature_C;
}

sbyte1 BMS_getLowestCellTemperature_C(BatteryManagementSystem *me)
{
    return me->lowestCellTemperature_C;
}

bool BMS_getRelayState(BatteryManagementSystem *me)
{
    return me->relayState;
}