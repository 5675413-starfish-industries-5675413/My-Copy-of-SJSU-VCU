#include <stdlib.h>
#include "shunt.h"

static ubyte4 Shunt_parseUbyte4LE(ubyte1* data);
static sbyte4 Shunt_parseSbyte4LE(ubyte1* data);
static unsigned long long Shunt_parseUint64LE(ubyte1* data);
static long long Shunt_parseInt64LE(ubyte1* data);

static ubyte2 Shunt_parseUbyte2BE(ubyte1* data);

Shunt *Shunt_new(void){
    Shunt *me = (Shunt *)malloc(sizeof(Shunt));
    if(me == NULL) return NULL;
    me->current_mA = 0;
    me->coulombCount = 0;
    return me;
}

void Shunt_parseCanMessage(Shunt *shunt, IO_CAN_DATA_FRAME *shuntCanMessage)
{
    switch(shuntCanMessage->id){
        case SHUNT_CURRENT_ID:
            shunt->current_mA = Shunt_parseSbyte4LE(shuntCanMessage->data);
            break;
        case SHUNT_TEMPERATURE_ID:
            shunt->temperature_dC = Shunt_parseSbyte4LE(shuntCanMessage->data);
            break;
        case SHUNT_VBUS_ID:
            shunt->vBus_mV = Shunt_parseSbyte4LE(shuntCanMessage->data);
            break;
        case SHUNT_COULOMB_ID:
            shunt->coulombCount = Shunt_parseInt64LE(shuntCanMessage->data);
            break;
        case SHUNT_POWER_ID:
            shunt->power_dW = Shunt_parseUbyte4LE(shuntCanMessage->data);
            break;
        case SHUNT_ENERGY_ID:
            shunt->energy_Wh = Shunt_parseUint64LE(shuntCanMessage->data);
            break;
        case SHUNT_ERRORS_ID:
            shunt->errors = Shunt_parseUbyte2BE(shuntCanMessage->data);
            break;
        default:
            break;
        }
}

static ubyte4 Shunt_parseUbyte4LE(ubyte1* data){
    return (ubyte4)(
        ((ubyte4) data[0])          |
        ((ubyte4) data[1] << 8)     |
        ((ubyte4) data[2] << 16)    |
        ((ubyte4) data[3] << 24)
    );
}

static sbyte4 Shunt_parseSbyte4LE(ubyte1* data){
    return (sbyte4) Shunt_parseUbyte4LE(data);
}

unsigned long long Shunt_parseUint64LE(ubyte1 *data){
    return (unsigned long long)(
        ((unsigned long long) data[0])        |
        ((unsigned long long) data[1] << 8)   |
        ((unsigned long long) data[2] << 16)  |
        ((unsigned long long) data[3] << 24)  |
        ((unsigned long long) data[4] << 32)  |
        ((unsigned long long) data[5] << 40)  |
        ((unsigned long long) data[6] << 48)  |
        ((unsigned long long) data[7] << 56)
    );
}

long long Shunt_parseInt64LE(ubyte1 *data){
    return (long long) Shunt_parseUint64LE(data);
}

ubyte2 Shunt_parseUbyte2BE(ubyte1 *data)
{
    return (ubyte2)(
        ((ubyte2) data[1])  |
        ((ubyte2) data[0] << 8)
    );
}
