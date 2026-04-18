#ifndef _DRS_H
#define _DRS_H

#include "IO_Driver.h"
#include "wheelSpeeds.h"
#include "brakePressureSensor.h"
#include "torqueEncoder.h"
#include "motorController.h"
#include "sensorCalculations.h"

// Rotary Switch modes:
//      Mode 0 - Always off
//      Mode 1 - Always on
//      Mode 2 - Driver controlled
//      Mode 3 - Assistive (manual & sensor controlled)
enum { STAY_CLOSED, STAY_OPEN, MANUAL, ASSISTIVE, AUTO };

//_DRS is a structure tag, used to reference the structure for future initializations 
typedef struct _DRS
{
    //flag indicating if DRS should be active or inactive based on inputs and mode
    bool AutoDRSActive; //used for debugging drs behavior on CAN

    //enum indicating current DRS mode selected based on rotaryswitch
    ubyte1 currentDRSMode;
    ubyte1 buttonPressed;
    ubyte1 drsFlapOpen;
    ubyte4 drsSafteyTimer;

    /* Hysteresis timers for AUTO mode (F1-style).
     * Conditions to open/close must be sustained for a minimum time to
     * avoid flap fluttering on noisy sensors or transient dips. */
    ubyte4 drsOpenConditionTimer;   /* timing how long open conditions have been met */
    ubyte4 drsCloseConditionTimer;  /* timing how long close conditions have been met */

} DRS;
//DRS is an instance of the _DRS struct
//what is the purpose of this initialized struct?

//initialize new DRS objects
DRS *DRS_new(void);

//DRS control logic
void DRS_update(DRS *me, MotorController *mcm, TorqueEncoder *tps, BrakePressureSensor *bps);
//actuate the wing to physically activate DRS
void DRS_open(DRS *me);
//retract the wing to physically deactivate DRS
void DRS_close(DRS *me);
// Checks for updates to Rotary Knob Position
void DRS_selectMode(DRS *me);

#endif