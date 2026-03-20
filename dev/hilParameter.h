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

// Parameter mapping elements
typedef struct {
    const char* name;
    void* address;
    ParamType type;
} ParamMapping;

// Encoding constants
#define MAX_NAME_LENGTH     6
#define BITS_PER_CHAR       5
#define CHAR_MASK           0x1F

/**********************************************************************/
/*** Populate parameter mapping table during runtime after struct   ***/ 
/*** object creation and before while loop                          ***/
/**********************************************************************/
void HIL_initParamTable(MotorController *mcm, PowerLimit *pl, LaunchControl *lc, WheelSpeeds *wss, BatteryManagementSystem *bms, Regen *regen, Efficiency *eff);

/**********************************************************************/
/*** Parse injected parameter from CAN (called from canManager.c)   ***/
/**********************************************************************/
void HIL_parseCanMessage(IO_CAN_DATA_FRAME* canMessage);

/**********************************************************************/
/*** Decode the encoded string value back to a string               ***/
/**********************************************************************/
void HIL_decodeName(ubyte4 encodedString, char *outputBuffer);

ubyte4 HIL_encodeName(const char *name);        // for debugging purposes only