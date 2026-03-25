#include <stdlib.h>
#include "shunt.h"

static ubyte4 Shunt_parseUbyte4(ubyte1* data);
static sbyte4 Shunt_parseSbyte4(ubyte1* data);
static long long Shunt_parseInt64(ubyte1* data);

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
            shunt->current_mA = Shunt_parseSbyte4(shuntCanMessage->data);
            break;
        case SHUNT_COULOMB_ID:
            shunt->coulombCount = Shunt_parseInt64(shuntCanMessage->data);
            break;
        default:
            break;
        }
}

static ubyte4 Shunt_parseUbyte4(ubyte1* data){
    return (ubyte4)(
        ((ubyte4) data[0])          |
        ((ubyte4) data[1] << 8)     |
        ((ubyte4) data[2] << 16)    |
        ((ubyte4) data[3] << 24)
    );
}

static sbyte4 Shunt_parseSbyte4(ubyte1* data){
    return (sbyte4) Shunt_parseUbyte4(data);
}

long long Shunt_parseInt64(ubyte1 *data)
{
    return (long long)(
        ((long long)data[0])        |
        ((long long)data[1] << 8)   |
        ((long long)data[2] << 16)  |
        ((long long)data[3] << 24)  |
        ((long long)data[4] << 32)  |
        ((long long)data[5] << 40)  |
        ((long long)data[6] << 48)  |
        ((long long)data[7] << 56)
    );
}
