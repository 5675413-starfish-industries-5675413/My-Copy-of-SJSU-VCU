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

// HIL States
typedef enum {
    HIL_STATE_DISCONNECTED = 0,
    HIL_STATE_DIAGNOSTIC   = 1,
    HIL_STATE_READY        = 2
} HIL_State;

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
/*** Parse incoming HIL command from CAN (called from canManager.c) ***/
/*** Byte 0 is the mux byte (command type)                          ***/
/***                                                                ***/
/*** CAN frame format:                                              ***/
/***   Byte 0:      0x01 (HIL_CMD_INIT)                             ***/
/***   Byte 1-7:    reserved                                        ***/
/***                                                                ***/
/***   Byte 0:      0x02 (HIL_CMD_INJECT)                           ***/
/***   Byte 1:      struct identifier  (ubyte1)                     ***/
/***   Byte 2:      param number       (ubyte1)                     ***/
/***   Byte 3:      reserved                                        ***/
/***   Bytes 4-7:   value              (32-bit, little-endian)      ***/
/***                                                                ***/
/***   Byte 0:      0x03 (HIL_CMD_REQUEST)                          ***/
/***   Byte 1:      struct identifier  (ubyte1)                     ***/
/***   Byte 2:      param number       (ubyte1)                     ***/
/***   Bytes 3-7:   reserved                                        ***/
/**********************************************************************/
void HIL_parseCanMessage(IO_CAN_DATA_FRAME* canMessage);

/**********************************************************************/
/*** Returns pointer to the response buffer. Called by              ***/
/*** canOutput_sendDebugMessage() in canManager.c                   ***/
/**********************************************************************/
IO_CAN_DATA_FRAME* HIL_getResponseBuffer(void);

/**********************************************************************/
/*** Build diagnostics response with HIL_RESP_DIAG frames            ***/
/*** To be sent from VCU to HIL on 0x514                            ***/
/**********************************************************************/
void HIL_buildDiagnostics(void);

/**********************************************************************/
/*** Build acknowledge response with HIL_RESP_ACK frame             ***/
/*** -> Send confirmation to HIL that param injection was successful***/
/*** To be sent from VCU to HIL on 0x514                            ***/
/**********************************************************************/
void HIL_buildAck(ubyte1 identifier, ubyte1 param_number, ubyte1 status);

/**********************************************************************/
/*** Build diagnostics response with HIL_RESP_DIAG frame            ***/
/*** To be sent from VCU to HIL on 0x514                            ***/
/**********************************************************************/
void HIL_buildValueResponse(ubyte1 identifier, ubyte1 param_number);