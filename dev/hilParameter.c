#include "IO_CAN.h"

#include "hilParameter.h"
#include "powerLimit.h"
#include "LaunchControl.h"
#include "bms.h"
#include "motorController.h"
#include "regen.h"
#include "efficiency.h"

void HIL_parseCanMessage(MotorController *mcm, PowerLimit *pl, LaunchControl *lc, BatteryManagementSystem *bms, Regen *regen, Efficiency *eff, IO_CAN_DATA_FRAME* HILCanMessage)
{
    // HIL Parameter Command Message Format
    ubyte2 paramID = HILCanMessage->data[0] | (HILCanMessage->data[1] << 8);    // maps to category and underlying parameter
    ubyte1 command = HILCanMessage->data[2];
    sbyte4 value = HILCanMessage->data[3] | (HILCanMessage->data[4] << 8) | (HILCanMessage->data[5] << 16) | (HILCanMessage->data[6] << 24);

    ubyte1 paramStatus = 0;     // 0: Success, 1: Invalid Parameter
    sbyte4 readValue = 0;       // If command = 0 (Read), assign current value read

    switch (paramID)
    {
        // Powerlimit Parameters
        case PARAM_PL_TOGGLE:
            if (command == 1) // Write
            {
                pl->plToggle = (bool)(value);
            }
            readValue = (sbyte4)(pl->plToggle);     // TODO: check if type casting is correct
            break;

        case PARAM_PL_STATUS:
            if (command == 1) // Write
            {
                pl->plStatus = (bool)(value);
            }
            readValue = (sbyte4)(pl->plStatus);     // TODO: check if type casting is correct
            break;
        
        case PARAM_PL_MODE:
            if (command == 1) // Write
            {
                pl->plMode = (ubyte1)(value);
            }
            readValue = (sbyte4)(pl->plMode);       // TODO: check if type casting is correct
            break;
    
        // .... other PowerLimit parameters

        // Launch Control Parameters
        case PARAM_LC_TOGGLE:
            if (command == 1) // Write
            {
                lc->lcToggle = (bool)(value);
            }
            readValue = (sbyte4)(lc->lcToggle);     // TODO: check if type casting is correct
            break;

        // .... other Launch Control parameters

        default:    // If CAN message value does not map to a valid parameter
            paramStatus = 1; // Invalid Parameter
            break;
    }

    // For debug purposes, send back a response message with the parameter value
    // HIL_sendResponse(paramID, paramStatus, readValue);
}