#ifndef _BATTERYMANAGEMENTSYSTEM_H
#define _BATTERYMANAGEMENTSYSTEM_H

#include <stdio.h>
#include <stdint.h>

#include "serial.h"
#include "IO_CAN.h"

#define BMS_MAX_CELL_MISMATCH_V 1.00f
#define BMS_MIN_CELL_VOLTAGE_WARNING 3.20f
#define BMS_MAX_CELL_TEMPERATURE_WARNING 55.0f

// BMS Transmitted Messages (BMS --> VCU)
#define BMS_PACK_SUMMARY_1                  0x622   //8 bytes
#define BMS_PACK_SUMMARY_2                  0x623   //8 bytes
#define BMS_HEARTBEAT_MESSAGE               0x6B1   //1 byte

typedef struct _BatteryManagementSystem BatteryManagementSystem;

BatteryManagementSystem* BMS_new(SerialManager* serialMan);
void BMS_parseCanMessage(BatteryManagementSystem* bms, IO_CAN_DATA_FRAME* bmsCanMessage);

IO_ErrorType BMS_relayControl(BatteryManagementSystem *me);
ubyte1 BMS_getFaultFlags(BatteryManagementSystem *me);
ubyte1 BMS_getWarningFlags(BatteryManagementSystem *me);
ubyte2 BMS_getHvSensePackVoltage_cV(BatteryManagementSystem *me);
sbyte2 BMS_getTotalPackVoltage_V(BatteryManagementSystem *me);
ubyte2 BMS_getStateOfCharge_mAh(BatteryManagementSystem *me);
sbyte2 BMS_getHighestCellVoltage_V(BatteryManagementSystem *me);
sbyte2 BMS_getLowestCellVoltage_V(BatteryManagementSystem *me);
sbyte1 BMS_getHighestCellTemperature_C(BatteryManagementSystem *me);
sbyte1 BMS_getLowestCellTemperature_C(BatteryManagementSystem *me);
ubyte2 BMS_getHeartbeatCount(BatteryManagementSystem *me);
bool BMS_getRelayState(BatteryManagementSystem *me);

typedef enum{
    BMS_CELL_OVER_VOLTAGE_FLAG = 0x01,
    BMS_CELL_UNDER_VOLTAGE_FLAG = 0x02,
    BMS_CELL_OVER_TEMPERATURE_FLAG = 0x04
} FaultFlags;

#endif // _BATTERYMANAGEMENTSYSTEM_H



