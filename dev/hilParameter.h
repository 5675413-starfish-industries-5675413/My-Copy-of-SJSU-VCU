#include <stddef.h>
#include "IO_CAN.h"
#include "IO_Driver.h"
#include "powerLimit.h"
#include "LaunchControl.h"
#include "wheelSpeeds.h"
#include "bms.h"
#include "motorController.h"
#include "regen.h"
#include "efficiency.h"

// Table size limits
#define HIL_MAX_PARAMS   64
#define HIL_MAX_STRUCTS  16

// Parameter data types
typedef enum {
    TYPE_BOOL,
    TYPE_UBYTE1,
    TYPE_UBYTE2,
    TYPE_UBYTE4,
    TYPE_SBYTE1,
    TYPE_SBYTE2,
    TYPE_SBYTE4,
    TYPE_FLOAT4
} ParamType;

// Single row in the parameter table
typedef struct {
    const char *struct_name;
    int identifier;
    int param_number;
    const char *param_name;
    void *mem_address;
    ParamType type;
} ParamRow;

// Parameter table with prefix sum for O(1) lookup
typedef struct {
    ParamRow rows[HIL_MAX_PARAMS];
    int sum[HIL_MAX_STRUCTS];
    int counts_length;
    int total_params;
} ParamTable;

/**********************************************************************/
/*** Populate parameter table during runtime after struct           ***/
/*** object creation and before while loop                          ***/
/**********************************************************************/
void HIL_initParamTable(MotorController *mcm, PowerLimit *pl, LaunchControl *lc, WheelSpeeds *wss, BatteryManagementSystem *bms, Regen *regen, Efficiency *eff);

/**********************************************************************/
/*** Parse injected parameter from CAN (called from canManager.c)   ***/
/***                                                                 ***/
/*** CAN frame format:                                               ***/
/***   Byte 0:   struct identifier  (ubyte1)                        ***/
/***   Byte 1:   param number       (ubyte1)                        ***/
/***   Bytes 2-3: reserved                                          ***/
/***   Bytes 4-7: value             (32-bit, little-endian)         ***/
/**********************************************************************/
void HIL_parseCanMessage(IO_CAN_DATA_FRAME* canMessage);
