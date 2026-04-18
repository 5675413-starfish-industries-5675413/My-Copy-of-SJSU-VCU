#include <stdlib.h>  //Needed for malloc
#include <math.h>
#include "IO_RTC.h"

#include "torqueEncoder.h"
#include "mathFunctions.h"

#include "sensors.h"
#include "virtualSensors.h"
extern Sensor Sensor_BenchTPS0;
extern Sensor Sensor_BenchTPS1;

/*****************************************************************************
* Torque Encoder (TPS) functions
* RULE EV2.3.5:
* If an implausibility occurs between the values of these two sensors the power to the motor(s) must be immediately shut down completely.
* It is not necessary to completely deactivate the tractive system, the motor controller(s) shutting down the power to the motor(s) is sufficient.
****************************************************************************/
TorqueEncoder* TorqueEncoder_new(bool benchMode)
{
    TorqueEncoder* me = (TorqueEncoder*)malloc(sizeof(struct _TorqueEncoder));
    //me->bench = benchMode;
    
    //TODO: Make sure the main loop is running before doing this
    //me->tps0 = (benchMode == TRUE) ? &Sensor_BenchTPS0 : &Sensor_TPS0;
    //me->tps1 = (benchMode == TRUE) ? &Sensor_BenchTPS1 : &Sensor_TPS1;
    me->tps0 = &Sensor_TPS0;
    me->tps1 = &Sensor_TPS1;

    //Where/should these be hardcoded?
    me->tps0_reverse = FALSE;
    me->tps1_reverse = TRUE;

    // TODO: Fetch from / store in EEPROM
    // exponent applied to pedal curve.  1 = linear, < 1 means more torque is given early on, > 1 means torque ramps up more slowly
    me->outputCurveExponent = 1.0;

    me->travelPercent = 0;
    me->travelPercent_raw = 0;
    /* EMA filter (exponential moving average / 1st-order low-pass):
     *   y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
     * alpha = dt / (tau + dt). With dt = 0.01s and tau ~= 0.03s (30ms),
     * alpha ~= 0.25. This cuts foot-bounce and ADC noise while adding only
     * ~30ms of lag -- imperceptible to the driver, but enough to stop the
     * traction-control PID from stuttering on raw signal jitter.
     */
    me->emaAlpha = 0.25f;
    me->runCalibration = FALSE;  //Do not run the calibration at the next main loop cycle

    //me->calibrated = FALSE;
    //TorqueEncoder_resetCalibration(me);

    //Old Rotary Sensor Datasheet limits (used by safety checker / rules requirement)
    //Datasheet had an operating range
    //me->tps0->specMin = 5000 * .05 - 5000 * .006; // = 220 , Target 0% = ~500
    //me->tps0->specMax = 5000 * .45 + 5000 * .006; // = 2280, Target 100% = ~2000
    //me->tps1->specMin = 5000 * .55 - 5000 * .006; // = 2720, Target 0% = ~3000
    //me->tps1->specMax = 5000 * .95 + 5000 * .006; // = 4780, Target 100% = ~4600

    //New Rotary Sensor Datasheet limits (used by safety checker / rules requirement)
    //It is literally a potentiometer, no sensor operating range in theory?
    //That would mean we could probably make our own ranges up
    me->tps0->specMin = 100; // Target 0% = ~250
    me->tps0->specMax = 4900; // Target 100% = ~2000
    me->tps1->specMin = 100; // Target 0% = ~2650
    me->tps1->specMax = 4900; // Target 100% = ~4700

    me->tps0_calibMin = 1000;
    me->tps0_calibMax = 2100;
    me->tps1_calibMin = 3100;
    me->tps1_calibMax = 4000;
    me->calibrated = TRUE;

    return me;
}

//Updates all values based on sensor readings, safety checks, etc
void TorqueEncoder_update(TorqueEncoder* me)
{
    if (me == NULL || me->tps0 == NULL || me->tps1 == NULL) { return; } /* MISRA null guard */

    me->tps0_value = me->tps0->sensorValue;
    me->tps1_value = me->tps1->sensorValue;

    /* -----------------------------------------------------------------
     * Virtual HIL override. When bench mode is latched (IO_DI_06 read at
     * boot by main.c), the ADC-derived tps*_value readings above are
     * IGNORED and we drive the pedal from the bench rig's last 0x5FE
     * CAN frame. The EMA filter still runs so downstream timing
     * matches real-vehicle behavior -- the only thing that changes is
     * the source of rawTravel.
     * ----------------------------------------------------------------- */
    if (VS_benchMode == TRUE)
    {
        float4 vapps = VS_apps_percent;
        if (vapps < 0.0f) { vapps = 0.0f; }
        if (vapps > 1.0f) { vapps = 1.0f; }

        me->tps0_percent      = vapps;
        me->tps1_percent      = vapps;
        me->travelPercent_raw = vapps;

        float4 alpha = me->emaAlpha;
        if (alpha <= 0.0f || alpha > 1.0f) { alpha = 1.0f; }
        me->travelPercent = (alpha * vapps)
                          + ((1.0f - alpha) * me->travelPercent);
        return;
    }

    ubyte2 errorCount = 0;

    /* If calibration is running, force 0% travel for safety */
    if (me->runCalibration == TRUE)
    {
        errorCount++;
        me->travelPercent_raw = 0;
        me->travelPercent     = 0;
        me->tps0_percent      = 0;
        me->tps1_percent      = 0;
    }
    else if (me->calibrated == FALSE)
    {
        me->tps0_percent      = 0;
        me->tps1_percent      = 0;
        me->travelPercent_raw = 0;
        me->travelPercent     = 0;
        (errorCount)++;
    }
    else
    {
        /* Per-sensor percentages: linear between calibMin and calibMax */
        me->tps0_percent = getPercent(me->tps0_value, me->tps0_calibMin, me->tps0_calibMax, TRUE);
        me->tps1_percent = getPercent(me->tps1_value, me->tps1_calibMin, me->tps1_calibMax, TRUE);

        /* Average of the two redundant APPS sensors (raw pedal travel) */
        float4 rawTravel = (me->tps0_percent + me->tps1_percent) / 2.0f;
        me->travelPercent_raw = rawTravel;

        /* -----------------------------------------------------------------
         * Exponential Moving Average (1st-order IIR low-pass) filter
         *   y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
         * This smooths mechanical foot-bounce and ADC electrical noise
         * before the value feeds the Traction Control PID, preventing
         * stutter on the torque command while adding only ~30ms lag.
         * -----------------------------------------------------------------
         */
        float4 alpha = me->emaAlpha;
        if (alpha <= 0.0f || alpha > 1.0f) { alpha = 1.0f; } /* safety */
        me->travelPercent = (alpha * rawTravel) + ((1.0f - alpha) * me->travelPercent);
    }
}

void TorqueEncoder_resetCalibration(TorqueEncoder* me)
{
    me->calibrated = FALSE;
    
    me->tps0_calibMin = me->tps0->sensorValue;
    me->tps0_calibMax = me->tps0->sensorValue;
    me->tps1_calibMin = me->tps1->sensorValue;
    me->tps1_calibMax = me->tps1->sensorValue;
}

void TorqueEncoder_saveCalibrationToEEPROM(TorqueEncoder* me)
{

}

void TorqueEncoder_loadCalibrationFromEEPROM(TorqueEncoder* me)
{

}

void TorqueEncoder_startCalibration(TorqueEncoder* me, ubyte1 secondsToRun)
{
    if (me->runCalibration == FALSE) //Ignore the button if calibration is already running
    {
        me->runCalibration = TRUE;
        TorqueEncoder_resetCalibration(me);
        me->calibrated = FALSE;
        IO_RTC_StartTime(&(me->timestamp_calibrationStart));
        me->calibrationRunTime = secondsToRun;
    }
    else
    {
        IO_RTC_StartTime(&(me->timestamp_calibrationStart));  //extend the calibration time
    }
}

/*-------------------------------------------------------------------
* CalibrateTPS
* Description: Records TPS minimum/maximum voltages (when?) and stores them (where?), or flags that calibration is complete
* Parameters:
* Inputs:
* Returns:
* Notes:
* Throws:
-------------------------------------------------------------------*/
// Physical pedal travel will only occur across the center (about 1/2) of the actual sensor's range of travel
// The rules (especially EV2.3.6) are written about % of PEDAL travel, not percent of sensor range, so we must calculate pedal travel by recording the min/max voltages at min/max throttle positions
void TorqueEncoder_calibrationCycle(TorqueEncoder* me, ubyte1* errorCount)
//THIS FUNCTION SHOULD NOT BE CALLED FROM MAIN
{
    if (me->runCalibration == TRUE)
    {
        if (IO_RTC_GetTimeUS(me->timestamp_calibrationStart) < (ubyte4)(me->calibrationRunTime) * 1000 * 1000)
        {
            //The calibration itself
            if (me->tps0->sensorValue < me->tps0_calibMin) { me->tps0_calibMin = me->tps0->sensorValue; }
            if (me->tps0->sensorValue > me->tps0_calibMax) { me->tps0_calibMax = me->tps0->sensorValue; }

            if (me->tps1->sensorValue < me->tps1_calibMin) { me->tps1_calibMin = me->tps1->sensorValue; }
            if (me->tps1->sensorValue > me->tps1_calibMax) { me->tps1_calibMax = me->tps1->sensorValue; }

        }
        else  //Calibration shutdown
        {
            //90 degree sensor active range.. so just say % = degrees
            //float4 pedalTopPlay = 1.05;
            //float4 pedalBottomPlay = .95;

            //Shrink the calibrated range slightly
            float4 shrink0 = (me->tps0_calibMax - me->tps0_calibMin) * .05;
            float4 shrink1 = (me->tps1_calibMax - me->tps1_calibMin) * .05;
            me->tps0_calibMin += shrink0;
            me->tps0_calibMax -= shrink0;
            me->tps1_calibMin += shrink1;
            me->tps1_calibMax -= shrink1;

            me->runCalibration = FALSE;
            me->calibrated = TRUE;
            Light_set(Light_dashEco, 0);

        }

    }
    else
    {
        //TODO: Throw warning: calibrationCycle helper function was called but calibration should not be running
    }

    //TODO: Write calibration data to EEPROM

    //TODO: Check for valid/reasonable calibration data

    //TODO: Do something on the display to show that voltages are being recorded

    //Idea: Display "bars" filling up on right segment (for gas pedal) _=E=_=E...
    //      Once calibration data makes sense, show pedal location (0-10%, 10-90%, 90-100%) with bars

}


void TorqueEncoder_getIndividualSensorPercent(TorqueEncoder* me, ubyte1 sensorNumber, float4* percent)
{
    switch (sensorNumber)
    {
    case 0:
        *percent = me->tps0_percent;
        break;
    case 1:
        *percent = me->tps1_percent;
        break;
    }
}


/*-------------------------------------------------------------------
* GetThrottlePosition
* Description: Reads TPS Pin voltages and returns % of throttle pedal travel.
* Parameters:  None
* Inputs:      Assumes TPS#.sensorValue has been set by main loop
* Returns:     Throttle value in percent (from 0 to 1)
* Notes:       Valid pedal travel is from 10% (0.10) to 90% (0.90), not including mechanical limits.
* Throws:      000 - TPS0 voltage out of range
*              001 - TPS1 voltage out of range, 002
-------------------------------------------------------------------*/
void TorqueEncoder_getPedalTravel(TorqueEncoder* me, ubyte1* errorCount, float4* pedalPercent)
{
    *pedalPercent = me->travelPercent;

    //What about other error states?
    //Voltage outside of calibration range
    //Voltages off center

    //    if (errorCount > 0)
    //    {
    //        return 0;
    //    }
    //    else
    //    {
    //return (TPS0PedalPercent + TPS1PedalPercent) / 2;
    //    }
}

void TorqueEncoder_getOutputPercent(TorqueEncoder* me, float4* outputPercent)
{
    *outputPercent = powf(me->travelPercent, me->outputCurveExponent);
}