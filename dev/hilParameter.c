#include "hilParameter.h"
#include "powerLimit.h"
#include "LaunchControl.h"
#include "wheelSpeeds.h"
#include "bms.h"
#include "motorController.h"
#include "regen.h"
#include "efficiency.h"
#include <string.h>

static const char HIL_CHARSET[] = " abcdefghijklmnopqrstuvwxyz01234";

static sbyte1 HIL_charToIndex(char c)
{
    // Handle uppercase letters and convert to lowercase
    if (c >= 'A' && c <= 'Z') {
        c = c - 'A' + 'a';
    }

    for (ubyte1 i = 0; i < 32; i++) {
        if (HIL_CHARSET[i] == c) {
            return (sbyte1)i;
        }
    }
    return -1;  // Invalid character
}

void HIL_decodeName(ubyte4 encodedString, char *outputBuffer)
{
    ubyte1 i;
    for (i = 0; i < MAX_NAME_LENGTH; i++) {
        ubyte1 shift = (MAX_NAME_LENGTH - 1 - i) * BITS_PER_CHAR;
        ubyte1 idx = (encodedString >> shift) & CHAR_MASK;

        if (idx == 0) {
            // Terminator reached
            break;
        }
        outputBuffer[i] = HIL_CHARSET[idx];
    }
    outputBuffer[i] = '\0'; // Null-terminate the string
}

ubyte4 HIL_encodeName(const char *name)
{
    ubyte1 len = strlen(name);
    if (len > MAX_NAME_LENGTH || len == 0) {
        return 0xFFFFFFFF; // Error indicator
    }

    ubyte4 encoded = 0;
    for (ubyte1 i = 0; i < len; i++) {
        sbyte1 idx = HIL_charToIndex(name[i]);
        if (idx < 0) {
            return 0xFFFFFFFF; // Error indicator
        }
        encoded = (encoded << BITS_PER_CHAR) | (ubyte4)idx;
    }

    // Fill remaining positions with zeros
    ubyte1 remaining = MAX_NAME_LENGTH - len;
    encoded <<= (remaining * BITS_PER_CHAR);

    return encoded;
}

/**********************************************************************/
/************************* PARAMETER TABLE ****************************/
/**********************************************************************/

#define HIL_MAX_PARAMS  64

static ParamMapping hilParamTable[HIL_MAX_PARAMS];
static size_t hilParamCount = 0;

// Add parameters to the lookup table
static void HIL_addParam(const char *nameStr, void *ptr, ParamType paramType)
{
    if (hilParamCount < HIL_MAX_PARAMS) {
        hilParamTable[hilParamCount].name = nameStr;
        hilParamTable[hilParamCount].address = ptr;
        hilParamTable[hilParamCount].type = paramType;
        hilParamCount++;
    }
}

// Called at runtime
void HIL_initParamTable(MotorController *mcm, PowerLimit *pl, LaunchControl *lc, WheelSpeeds *wss, BatteryManagementSystem *bms, Regen *regen, Efficiency *eff)
{
    hilParamCount = 0;

    // MotorController parameters
    // !! Modify parameters as needed !!
    
    // PowerLimit parameters
    HIL_addParam("pltog",      &(pl->plToggle),                    TYPE_BOOL);
    HIL_addParam("plstat",     &(pl->plStatus),                    TYPE_BOOL);
    HIL_addParam("plmode",     &(pl->plMode),                      TYPE_UBYTE1);
    HIL_addParam("pltarg",     &(pl->plTargetPower),               TYPE_UBYTE1);
    HIL_addParam("plkwlm",     &(pl->plKwLimit),                   TYPE_UBYTE1);
    HIL_addParam("plinit",     &(pl->plInitializationThreshold),   TYPE_UBYTE1);
    HIL_addParam("pltqcm",     &(pl->plTorqueCommand),             TYPE_SBYTE2);
    HIL_addParam("plclmp",     &(pl->clampingMethod),              TYPE_UBYTE1);
    // !! Modify parameters as needed !!

    // LaunchControl (and WheelSpeedSensor) parameters
    HIL_addParam("lctog",      &(lc->lcToggle),                    TYPE_BOOL);
    HIL_addParam("lcstat",     &(lc->state),                       TYPE_SBYTE4);   // !! Need to check type !!
    HIL_addParam("lcmode",     &(lc->mode),                        TYPE_SBYTE4);   // !! Need to check type !!
    HIL_addParam("lckp",       &(lc->Kp),                          TYPE_SBYTE1);
    HIL_addParam("lcki",       &(lc->Ki),                          TYPE_SBYTE1);
    HIL_addParam("lckd",       &(lc->Kd),                          TYPE_SBYTE1);
    HIL_addParam("lcsrt",      &(lc->slipRatioTarget),             TYPE_FLOAT4);
    HIL_addParam("lcinit",     &(lc->initialTorque),               TYPE_FLOAT4);
    HIL_addParam("lcmaxt",     &(lc->maxTorque),                   TYPE_FLOAT4);
    HIL_addParam("lck",        &(lc->k),                           TYPE_FLOAT4);
    HIL_addParam("lcfilt",     &(lc->useFilter),                   TYPE_BOOL);

    // Placeholders for WSS RPM and speed parameters
    //HIL_addParam("fastestrearrpm",    &(wss->fastestRearRPM),          TYPE_FLOAT4);
    //HIL_addParam("groundspeedrpm",     &(wss->groundSpeedRPM),          TYPE_FLOAT4);
    //HIL_addParam("groundspeedmps",    &(wss->groundSpeedMPS),          TYPE_FLOAT4);
    //HIL_addParam("fastestrearmps",    &(wss->fastestRearMPS),          TYPE_FLOAT4);
    // !! Modify parameters as needed !!

    // BMS parameters
    // !! Modify parameters as needed !!

    // Regen parameters
    // !! Modify parameters as needed !!

    // Efficiency parameters
    HIL_addParam("eftog",      &(eff->efficiencyToggle),           TYPE_BOOL);
    HIL_addParam("efbudg",     &(eff->energyBudget_kWh),           TYPE_FLOAT4);
    HIL_addParam("effin",      &(eff->finishedLap),                TYPE_BOOL);
    HIL_addParam("efdist",     &(eff->lapDistance_km),             TYPE_FLOAT4);
    // !! Modify parameters as needed !!
}

// Parameter lookup function
static ParamMapping* HIL_findParamByName(const char *name)
{
    for (size_t i = 0; i < hilParamCount; i++) {
        if (strcmp(hilParamTable[i].name, name) == 0) {
            return &hilParamTable[i];
        }
    }
    return NULL; // Not found
}

// Write parameter into struct
static void HIL_writeParam(void *addr, ParamType type, sbyte4 value)
{
    switch (type) {
        case TYPE_BOOL:     *(bool*)(addr) = (bool)value;                 break;
        case TYPE_UBYTE1:   *(ubyte1*)(addr) = (ubyte1)value;             break;
        case TYPE_UBYTE2:   *(ubyte2*)(addr) = (ubyte2)value;             break;
        case TYPE_UBYTE4:   *(ubyte4*)(addr) = (ubyte4)value;             break;
        case TYPE_SBYTE1:   *(sbyte1*)(addr) = (sbyte1)value;             break;
        case TYPE_SBYTE2:   *(sbyte2*)(addr) = (sbyte2)value;             break;
        case TYPE_SBYTE4:   *(sbyte4*)(addr) = value;                     break;
        case TYPE_FLOAT4:   memcpy(addr, &value, sizeof(float4));         break;
    }
}

// Parse injected parameter from CAN
void HIL_parseCanMessage(IO_CAN_DATA_FRAME* canMessage)
{
    // HIL Parameter Command Message Format
    ubyte4 encodedName = ((ubyte4)canMessage->data[0] << 24) |
                         ((ubyte4)canMessage->data[1] << 16) |      // Byte 0-3: Encoded string value (32 bits, big-endian)
                         ((ubyte4)canMessage->data[2] << 8)  |
                         ((ubyte4)canMessage->data[3]);

    sbyte4 value = (sbyte4)(
                   ((ubyte4)canMessage->data[4])       |
                   ((ubyte4)canMessage->data[5] << 8)  |            // Byte 4-7: Parameter value (32 bits, signed, little-endian)
                   ((ubyte4)canMessage->data[6] << 16) |
                   ((ubyte4)canMessage->data[7] << 24));

    // Decode parameter name
    char paramName[MAX_NAME_LENGTH + 1];
    HIL_decodeName(encodedName, paramName);

    // Find parameter in table
    ParamMapping *mapping = HIL_findParamByName(paramName);
    if (mapping == NULL) {
        // Invalid parameter, cannot be found
        return;
    }

    // Overwrite struct parameter
    HIL_writeParam(mapping->address, mapping->type, value);

    // For debugging purposes:
    // HIL_sendResponse(encodedName, value);
}