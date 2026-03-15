#ifndef _SHUNT_H
#define _SHUNT_H

#include "IO_Driver.h"
#include "IO_CAN.h"

#define SHUNT_CURRENT_ID            0x3F1
#define SHUNT_COULOMB_ID            0x3F4
#define SHUNT_SET_COMMAND_ID        0x3FA
#define SHUNT_GET_COMMAND_ID        0x3FB
#define SHUNT_REPLY_ID              0x3FC

typedef struct {
    sbyte4 high;
    ubyte4 low;
} split_sbyte8;

typedef struct _Shunt{
    sbyte4 current_mA;
    split_sbyte8 coulombCount;
} Shunt;

Shunt *Shunt_new(void);
void Shunt_parseCanMessage(Shunt *shunt, IO_CAN_DATA_FRAME *shuntCanMessage);

#endif