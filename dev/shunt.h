#ifndef _SHUNT_H
#define _SHUNT_H

#include "IO_Driver.h"
#include "IO_CAN.h"

#define SHUNT_CURRENT_ID            0x3F1
#define SHUNT_TEMPERATURE_ID        0x3F2
#define SHUNT_VBUS_ID               0x3F3
#define SHUNT_COULOMB_ID            0x3F4
#define SHUNT_POWER_ID              0x3F5
#define SHUNT_ENERGY_ID             0x3F6
#define SHUNT_ERRORS_ID             0x3F7

typedef enum{
    SHUNT_COMMAND_GET_ALL_ENABLED    = 0x00,
    SHUNT_COMMAND_GET_CURRENT       = 0x01,
    SHUNT_COMMAND_GET_TEMPERATURE   = 0x02,
    SHUNT_COMMAND_GET_VBUS          = 0x03,
    SHUNT_COMMAND_GET_COULOMB       = 0x04,
    SHUNT_COMMAND_GET_POWER         = 0x05,
    SHUNT_COMMAND_GET_ENERGY        = 0x06,
    SHUNT_COMMAND_GET_ERRORS        = 0x07,
    SHUNT_COMMAND_RESET             = 0x10,
    SHUNT_COMMAND_SETMODE           = 0x12,
    SHUNT_COMMAND_READING_DELAY     = 0x16
} ShuntCommand;


typedef struct _Shunt{
    sbyte4 current_mA;
    sbyte4 temperature_dC;
    sbyte4 vBus_mV;
    long long coulombCount;
    ubyte4 power_dW;
    unsigned long long energy_Wh;
    ubyte2 errors;
} Shunt;

Shunt *Shunt_new(void);
void Shunt_parseCanMessage(Shunt *shunt, IO_CAN_DATA_FRAME *shuntCanMessage);

#endif