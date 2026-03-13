#include "hilParameter.h"
#include "powerLimit.h"
#include "LaunchControl.h"
#include "wheelSpeeds.h"
#include "bms.h"
#include "motorController.h"
#include "regen.h"
#include "efficiency.h"
#include <stdlib.h>
#include <string.h>

/**********************************************************************/
/************************* PARAMETER TABLE ****************************/
/**********************************************************************/

static ParamTable hilParamTable;

static int compare_by_structname(const void *a, const void *b)
{
    return strcmp(((const ParamRow *)a)->struct_name, ((const ParamRow *)b)->struct_name);
}

static int compare_by_paramname(const void *a, const void *b)
{
    return strcmp(((const ParamRow *)a)->param_name, ((const ParamRow *)b)->param_name);
}

// Called at runtime
void HIL_initParamTable(MotorController *mcm, PowerLimit *pl, LaunchControl *lc, WheelSpeeds *wss, BatteryManagementSystem *bms, Regen *regen, Efficiency *eff)
{
    ParamRow rows[HIL_MAX_PARAMS];
    int n = 0;

    // ParamTable row format:
    // { struct_name, identifier, param_number, param_name, &struct->field (memory address), type }

    // PowerLimit parameters
    // !! Modify parameters as needed !!
    rows[n++] = (ParamRow){ "PowerLimit", 0, 0, "plToggle",                  &(pl->plToggle),                  TYPE_BOOL   };
    rows[n++] = (ParamRow){ "PowerLimit", 0, 0, "plStatus",                  &(pl->plStatus),                  TYPE_BOOL   };
    rows[n++] = (ParamRow){ "PowerLimit", 0, 0, "plMode",                    &(pl->plMode),                    TYPE_UBYTE1 };
    rows[n++] = (ParamRow){ "PowerLimit", 0, 0, "plTargetPower",             &(pl->plTargetPower),             TYPE_UBYTE1 };
    rows[n++] = (ParamRow){ "PowerLimit", 0, 0, "plKwLimit",                 &(pl->plKwLimit),                 TYPE_UBYTE1 };
    rows[n++] = (ParamRow){ "PowerLimit", 0, 0, "plInitializationThreshold", &(pl->plInitializationThreshold), TYPE_UBYTE1 };
    rows[n++] = (ParamRow){ "PowerLimit", 0, 0, "plTorqueCommand",           &(pl->plTorqueCommand),           TYPE_SBYTE2 };
    rows[n++] = (ParamRow){ "PowerLimit", 0, 0, "clampingMethod",            &(pl->clampingMethod),            TYPE_UBYTE1 };
    // !! Modify parameters as needed !!

    // LaunchControl parameters
    // !! Modify parameters as needed !!
    rows[n++] = (ParamRow){ "LaunchControl", 0, 0, "lcToggle",        &(lc->lcToggle),        TYPE_BOOL   };
    rows[n++] = (ParamRow){ "LaunchControl", 0, 0, "state",           &(lc->state),           TYPE_SBYTE4 };   // !! Need to check type !!
    rows[n++] = (ParamRow){ "LaunchControl", 0, 0, "mode",            &(lc->mode),            TYPE_SBYTE4 };   // !! Need to check type !!
    rows[n++] = (ParamRow){ "LaunchControl", 0, 0, "Kp",              &(lc->Kp),              TYPE_SBYTE1 };
    rows[n++] = (ParamRow){ "LaunchControl", 0, 0, "Ki",              &(lc->Ki),              TYPE_SBYTE1 };
    rows[n++] = (ParamRow){ "LaunchControl", 0, 0, "Kd",              &(lc->Kd),              TYPE_SBYTE1 };
    rows[n++] = (ParamRow){ "LaunchControl", 0, 0, "slipRatioTarget", &(lc->slipRatioTarget), TYPE_FLOAT4 };
    rows[n++] = (ParamRow){ "LaunchControl", 0, 0, "initialTorque",   &(lc->initialTorque),   TYPE_FLOAT4 };
    rows[n++] = (ParamRow){ "LaunchControl", 0, 0, "maxTorque",       &(lc->maxTorque),       TYPE_FLOAT4 };
    rows[n++] = (ParamRow){ "LaunchControl", 0, 0, "k",               &(lc->k),               TYPE_FLOAT4 };
    rows[n++] = (ParamRow){ "LaunchControl", 0, 0, "useFilter",       &(lc->useFilter),       TYPE_BOOL   };
    // !! Modify parameters as needed !!

    // WheelSpeeds parameters
    // !! Modify parameters as needed !!
    //rows[n++] = (ParamRow){ "WheelSpeeds", 0, 0, "fastestRearMPS", &(wss->fastestRearMPS), TYPE_FLOAT4 };

    // BMS parameters
    // !! Modify parameters as needed !!

    // Regen parameters
    // !! Modify parameters as needed !!

    // Efficiency parameters
    // !! Modify parameters as needed !!
    rows[n++] = (ParamRow){ "Efficiency", 0, 0, "efficiencyToggle", &(eff->efficiencyToggle), TYPE_BOOL   };
    rows[n++] = (ParamRow){ "Efficiency", 0, 0, "energyBudget_kWh", &(eff->energyBudget_kWh), TYPE_FLOAT4 };
    rows[n++] = (ParamRow){ "Efficiency", 0, 0, "finishedLap",      &(eff->finishedLap),      TYPE_BOOL   };
    rows[n++] = (ParamRow){ "Efficiency", 0, 0, "lapDistance_km",   &(eff->lapDistance_km),   TYPE_FLOAT4 };
    // !! Modify parameters as needed !!

    int total = n;

    /* --- Step 1: Sort by struct name to group entries --- */
    qsort(rows, total, sizeof(ParamRow), compare_by_structname);

    /* --- Step 2: Assign identifiers and count group sizes --- */
    int counts[HIL_MAX_STRUCTS];
    int counts_length = 0;
    int count = 1;
    int identifier = 0;
    rows[0].identifier = 0;

    for (int i = 1; i < total; i++) {
        if (strcmp(rows[i].struct_name, rows[i-1].struct_name) != 0) {
            counts[counts_length++] = count;
            count = 1;
            identifier++;
        } else {
            count++;
        }
        rows[i].identifier = identifier;
    }
    counts[counts_length++] = count;

    /* --- Step 3: Build prefix sum (boundary index of each struct group) --- */
    int sum[HIL_MAX_STRUCTS];
    int acc = 0;
    for (int i = 0; i < counts_length; i++) {
        acc += counts[i];
        sum[i] = acc;
    }

    /* --- Step 4: Sort within each struct group by param name --- */
    qsort(rows, counts[0], sizeof(ParamRow), compare_by_paramname);
    for (int g = 1; g < counts_length; g++) {
        qsort(rows + sum[g-1], counts[g], sizeof(ParamRow), compare_by_paramname);
    }

    /* --- Step 5: Assign param_number within each group --- */
    rows[0].param_number = 0;
    for (int i = 1; i < total; i++) {
        if (strcmp(rows[i].struct_name, rows[i-1].struct_name) != 0) {
            rows[i].param_number = 0;
        } else {
            rows[i].param_number = rows[i-1].param_number + 1;
        }
    }

    /* --- Store into static table --- */
    memcpy(hilParamTable.rows, rows, total * sizeof(ParamRow));
    memcpy(hilParamTable.sum, sum, counts_length * sizeof(int));
    hilParamTable.counts_length = counts_length;
    hilParamTable.total_params = total;
}

// Parameter lookup by (identifier, param_number) — O(1) via prefix sum
static ParamRow *HIL_findParam(int identifier, int param_number)
{
    if (identifier < 0 || identifier >= hilParamTable.counts_length) {
        return NULL;
    }

    int index = (identifier == 0) ? param_number
                                  : hilParamTable.sum[identifier - 1] + param_number;

    if (index < 0 || index >= hilParamTable.total_params) {
        return NULL;
    }

    if (hilParamTable.rows[index].identifier != identifier) {
        return NULL;
    }

    return &hilParamTable.rows[index];
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
    // HIL Parameter Command Message Format:
    ubyte1 identifier   = canMessage->data[0];      // Byte 0: struct identifier
    ubyte1 param_number = canMessage->data[1];      // Byte 1: param number
                                                    // Bytes 2-3: reserved
    sbyte4 value = (sbyte4)(
                   ((ubyte4)canMessage->data[4])        |
                   ((ubyte4)canMessage->data[5] << 8)   |   // Bytes 4-7: value (32-bit, little-endian)
                   ((ubyte4)canMessage->data[6] << 16)  |
                   ((ubyte4)canMessage->data[7] << 24));

    ParamRow *row = HIL_findParam(identifier, param_number);
    if (row == NULL) {
        return;
    }

    HIL_writeParam(row->mem_address, row->type, value);

    // For debugging purposes: (if we want to have it)
    // HIL_sendResponse(identifier, param_number, value);
}
