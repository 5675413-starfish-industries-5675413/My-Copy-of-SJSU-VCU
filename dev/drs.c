#include <stdlib.h>
#include "IO_Driver.h"
#include "IO_DIO.h"
#include "IO_RTC.h"

#include "drs.h"
#include "sensors.h"
#include "wheelSpeeds.h"
#include "brakePressureSensor.h"
#include "torqueEncoder.h"
#include "motorController.h"
#include "sensorCalculations.h"

extern Sensor Sensor_DRSButton; 
extern Sensor Sensor_DRSKnob;

DRS *DRS_new(void)
{
    DRS *me = (DRS *)malloc(sizeof(struct _DRS));
    if (me == NULL) { return NULL; } /* MISRA null guard */

    me->AutoDRSActive          = FALSE;
    me->currentDRSMode         = MANUAL;
    me->buttonPressed          = FALSE;
    me->drsFlapOpen            = FALSE;
    me->drsSafteyTimer         = 0;
    me->drsOpenConditionTimer  = 0;
    me->drsCloseConditionTimer = 0;

    return me;
}

//----------------------------------------------------------------------
// Drag reduction system calculations.
// First, check the mode indicated by the driver rotary switch
//      Mode 0 - Always off
//      Mode 1 - Always on
//      Mode 2 - Driver controlled
//      Mode 3 - Auto (sensor controlled)
// Second, set AutoDRSActive flag
// **DOES NOT** physically activate the wing
//
// Auto Params:
//    Enable: 
//      Throttle Position > 80%
//      Wheel Speed > 30 mph
//      Steering Angle < 5% off 0 degrees
//      
//    Disable:
//      Brake Pressure 
//----------------------------------------------------------------------


void DRS_update(DRS *me, MotorController *mcm, TorqueEncoder *tps, BrakePressureSensor *bps) {
    
    // Checks for updates to Rotary Knob Position
    // DRS_selectMode(me);

    sbyte2 steeringAngle = steering_degrees(); // < +/- 15 deg
    float4 bpsPercent = bps->percent; // > 20%
    float4 appsPercent = tps->travelPercent; // > 90%
    //For AutoDRS
    sbyte2 groundspeedMPH = 0.62 * MCM_getGroundSpeedKPH(mcm); // >30mph
    //button check for all later cases where it might be needed
    if(Sensor_DRSButton.sensorValue) { 
        me->buttonPressed = TRUE;
    }
    else {
        me->buttonPressed = FALSE;
    }

    me->currentDRSMode = MANUAL; 

    switch(me->currentDRSMode)
        {
            case STAY_CLOSED:
                DRS_close(me);
                break;

            case STAY_OPEN:
                DRS_open(me);           
                break;

            case MANUAL:
                if(me->buttonPressed) {
                    DRS_open(me); }
                else {
                    DRS_close(me); }
                break;

            case ASSISTIVE:
                /* 
                1. To open, check if button is pressed & DRS flap closed (should be false) & Have waited at least 5 cycles (50ms)
                2. Restar timer & Open DRS
                3. To close, check if button is pressed & DRS engaged (should be TRUE) & Have waited at least 5 cycles (50ms)
                4. Restart timer & Close DRS
                5. EXIT CONDITONS: (Brake pressure is > 20% or steering angle is +/- 15° ) */

                // conditions are listed in prioritised order, check flap state first (put ourselves in the relevant if statement), then button press, then timer
                if(!me->drsFlapOpen && me->buttonPressed && IO_RTC_GetTimeUS(me->drsSafteyTimer) >= 45000 ){ //check if button press && drs flap is closed && drsSafety Timer Passed > 0.05s, using 0.45s in case the vcu cycle time is somehow off by a microsecond or two (0.00001! lol)
                    IO_RTC_StartTime(&me->drsSafteyTimer); //restart timer
                    DRS_open(me);
                }
                // conditions are listed in prioritised order
                else if(me->drsFlapOpen && me->buttonPressed && IO_RTC_GetTimeUS(me->drsSafteyTimer) >= 10000 ){ //check if button press && drs flap is open && drsSafety Timer Passed > 0.01s, sohrter interval bc prioritising close over open
                    IO_RTC_StartTime(&me->drsSafteyTimer); //restart timer
                    DRS_close(me);
                }

                if(me->drsFlapOpen && (bpsPercent > .20 || steeringAngle > 15 || steeringAngle < -15)){ //check if [ bps > 20% or steering angle > +/- 15deg ] and drs is open 
                    // IO_RTC_StartTime(&me->drsSafteyTimer); //restart timer? or allow immediate correcting in edge cases...
                    DRS_close(me);
                }
                break;

            case AUTO:
                /* ------------------------------------------------------------------
                 * F1-style DRS with aerodynamic hysteresis
                 * ------------------------------------------------------------------
                 * OPEN conditions (must ALL hold for >= 250 ms):
                 *      groundspeed > 25 mph
                 *      throttle    > 85%
                 *      |steering|  < 10 deg
                 *      brake       <= 0 (pedal essentially untouched)
                 *
                 * CLOSE (immediate): driver touches brakes OR lifts off throttle
                 * (lift <= 5% or brake > 5%). This prevents the flap from fluttering
                 * on small steering / throttle dips mid-corner-exit.
                 * Once open it stays open until a genuine brake/lift event.
                 * ------------------------------------------------------------------ */
                {
                    bool openConditions =
                           (groundspeedMPH > 25)
                        && (appsPercent    > 0.85f)
                        && (steeringAngle >  -10)
                        && (steeringAngle <   10)
                        && (bpsPercent    <= 0.05f);

                    /* Instant close when driver clearly wants to slow / turn. */
                    bool closeCommand =
                           (bpsPercent > 0.05f)     /* driver on brakes */
                        || (appsPercent < 0.05f);   /* driver lifted off throttle */

                    if (me->drsFlapOpen == FALSE) {
                        /* Flap is currently CLOSED: debounce open conditions.
                         * Require openConditions to hold sustained for 250ms. */
                        if (openConditions) {
                            if (me->drsOpenConditionTimer == 0) {
                                IO_RTC_StartTime(&me->drsOpenConditionTimer);
                            } else if (IO_RTC_GetTimeUS(me->drsOpenConditionTimer) >= 250000) {
                                me->AutoDRSActive = TRUE;
                                DRS_open(me);
                                me->drsOpenConditionTimer  = 0;
                                me->drsCloseConditionTimer = 0;
                            }
                        } else {
                            me->drsOpenConditionTimer = 0; /* reset if any condition fails */
                        }
                    } else {
                        /* Flap is currently OPEN. Stay open until a close command
                         * arrives -- ignore transient steering/throttle dips. */
                        if (closeCommand) {
                            me->AutoDRSActive = FALSE;
                            DRS_close(me);
                            me->drsOpenConditionTimer  = 0;
                            me->drsCloseConditionTimer = 0;
                        }
                    }
                }
                break;
            default:
                break;
        }
}

void DRS_open(DRS *me) {
    IO_DO_Set(IO_DO_06, TRUE);
    IO_DO_Set(IO_DO_07, FALSE);
    me->drsFlapOpen = 1;
}

void DRS_close(DRS *me) {
    IO_DO_Set(IO_DO_06, FALSE);
    IO_DO_Set(IO_DO_07, TRUE);
    me->drsFlapOpen = 0;
}

//Change to future regarding rotary voltage values
void DRS_selectMode(DRS *me) {
        if (Sensor_DRSKnob.sensorValue == 0)
        {    me->currentDRSMode = STAY_CLOSED;}
        else if (Sensor_DRSKnob.sensorValue <= 1.1)
        {    me->currentDRSMode = MANUAL;}
        else if (Sensor_DRSKnob.sensorValue <= 2.2)
        {    me->currentDRSMode = ASSISTIVE;}
        else if (Sensor_DRSKnob.sensorValue <= 3.3)
        {    me->currentDRSMode = STAY_OPEN;}
        else if (Sensor_DRSKnob.sensorValue > 3.3)
        {    me->currentDRSMode = STAY_CLOSED;}
}
