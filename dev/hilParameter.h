#include "IO_CAN.h"

#include "powerLimit.h"
#include "LaunchControl.h"
#include "bms.h"
#include "motorController.h"
#include "regen.h"
#include "efficiency.h"

typedef enum {
    // PowerLimit ->        0x0100 - 0x01FF
    PARAM_PL_TOGGLE         = 0x0100,
    PARAM_PL_STATUS         = 0x0101,
    PARAM_PL_MODE           = 0x0102,

    // Launch Control ->    0x0200 - 0x02FF
    PARAM_LC_TOGGLE         = 0x0200,


    // Regen ->             0x0300 - 0x03FF
} ParameterID;

void HIL_parseCanMessage(MotorController *mcm, PowerLimit *pl, LaunchControl *lc, BatteryManagementSystem *bms, Regen *regen, Efficiency *eff, IO_CAN_DATA_FRAME* HILCanMessage);