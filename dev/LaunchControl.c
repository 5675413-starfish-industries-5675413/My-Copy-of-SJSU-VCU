#include <stdlib.h>
#include <math.h>
#include "IO_RTC.h"
#include "IO_DIO.h"
#include "LaunchControl.h"
#include "wheelSpeeds.h"
#include "mathFunctions.h"
#include "initializations.h"
#include "sensors.h"
#include "torqueEncoder.h"
#include "brakePressureSensor.h"
#include "motorController.h"
#include "sensorCalculations.h"

extern Sensor Sensor_LCButton;
extern Sensor Sensor_DRSKnob;

/*
 * Calctorque holds the last PID-calculated torque REDUCTION (DNm), surfaced
 * on CAN for telemetry via getCalculatedTorque().
 */
static float4 Calctorque = 0;

/* ------------------------------------------------------------------
 * PID Controller
 * ------------------------------------------------------------------
 * The controller acts on slip ratio error (target - current). When actual
 * slip exceeds the target, error is negative, so the output must be inverted
 * to give a POSITIVE torque reduction. That inversion is done in the caller.
 *
 * Physics intuition:
 *   error = targetSlip - measuredSlip   [unitless slip ratio]
 *   P-term ~ how aggressively we cut torque for slip overshoot now
 *   I-term ~ removes steady-state offset (persistent slip)
 *   D-term ~ damps fast slip spikes from wheel hop / bumps
 * ------------------------------------------------------------------ */
void initPIDController(PIDController *controller, float4 p, float4 i, float4 d) {
    controller->kp        = p;
    controller->ki        = i;
    controller->kd        = d;
    controller->errorSum  = 0.0f;
    controller->lastError = 0.0f;
}

/*
 * Returns a POSITIVE torque reduction (DNm) to subtract from driver request.
 * 'target' is the slip ratio setpoint (e.g. 0.15), 'current' is measured slip,
 * dt is loop period in seconds (0.01 for 10ms loop).
 */
float4 calculatePIDReduction(PIDController *controller, float4 target, float4 current, float4 dt, sbyte2 maxReductionDNm) {
    /* Positive slipError => we are slipping too much and must REDUCE torque */
    float4 slipError = current - target;

    /* Proportional term */
    float4 pTerm = controller->kp * slipError;

    /* Integral term with trapezoidal-equivalent accumulation (dt = 0.01s) */
    controller->errorSum += slipError * dt;

    /* Clamp integrator to prevent windup (bounded by maxReduction / ki) */
    if (controller->ki > 0.0001f) {
        float4 integLimit = (float4)maxReductionDNm / controller->ki;
        if (controller->errorSum >  integLimit) { controller->errorSum =  integLimit; }
        if (controller->errorSum < -integLimit) { controller->errorSum = -integLimit; }
    }
    float4 iTerm = controller->ki * controller->errorSum;

    /* Derivative of slip error. MISRA-C: guard divide-by-zero on dt. */
    float4 dTerm = 0.0f;
    if (dt > 0.0001f) {
        dTerm = controller->kd * (slipError - controller->lastError) / dt;
    }
    controller->lastError = slipError;

    float4 reduction = pTerm + iTerm + dTerm;

    /* Output is a REDUCTION; only positive values mean "cut torque" */
    if (reduction < 0.0f) {
        reduction = 0.0f;
    }
    if (reduction > (float4)maxReductionDNm) {
        reduction = (float4)maxReductionDNm;
    }
    return reduction;
}

/* ------------------------------------------------------------------
 * Launch Control lifecycle
 * ------------------------------------------------------------------ */
LaunchControl *LaunchControl_new(void)
{
    LaunchControl *me = (LaunchControl *)malloc(sizeof(struct _LaunchControl));
    if (me == NULL) { return NULL; } /* MISRA null-pointer guard */

    me->slipRatio             = 0.0f;
    me->targetSlip            = 0.15f;   /* 15% target slip for max forward bite */
    me->lcTorqueReductionDNm  = 0;
    me->LCReady               = FALSE;
    me->LCStatus              = FALSE;
    me->pidController         = (PIDController *)malloc(sizeof(struct _PIDController));
    if (me->pidController == NULL) {
        free(me);
        return NULL;
    }
    initPIDController(me->pidController, 0.0f, 0.0f, 0.0f);
    me->buttonDebug = 0;
    return me;
}

/* ------------------------------------------------------------------
 * Slip Ratio Calculation
 * ------------------------------------------------------------------
 * slipRatio = (Driven - Undriven) / Undriven
 *   Driven   = rear wheels (fastest rear, RWD car)
 *   Undriven = front wheels (slowest front, ground-truth speed)
 *
 * If the car is at a standstill the undriven speed is ~0, so we MUST guard
 * against divide-by-zero (MISRA 21.1 / rule 4). Below a minimum undriven
 * speed, slip is treated as 0 (no traction control action).
 * ------------------------------------------------------------------ */
void slipRatioCalculation(WheelSpeeds *wss, LaunchControl *me)
{
    if (wss == NULL || me == NULL) { return; }

    float4 undriven = WheelSpeeds_getSlowestFront(wss); /* front (m/s) */
    float4 driven   = WheelSpeeds_getFastestRear(wss);  /* rear  (m/s) */

    /* Guard: below ~1 m/s (3.6 kph) wheel speed is too noisy AND we avoid
     * divide-by-zero at a standstill (launch control init condition). */
    float4 slip = 0.0f;
    if (undriven > 1.0f) {
        slip = (driven - undriven) / undriven;
    }

    /* Saturate to physically sane bounds [-1, 1] */
    if (slip >  1.0f) { slip =  1.0f; }
    if (slip < -1.0f) { slip = -1.0f; }

    me->slipRatio = slip;
}

/* ------------------------------------------------------------------
 * Launch Control torque reduction calculation
 * ------------------------------------------------------------------
 * Flow:
 *   1. Arm (LCReady) when driver holds LC button at standstill with brakes.
 *   2. When driver releases LC button and pins throttle, LCStatus goes TRUE.
 *   3. While LCStatus, the PID computes a REDUCTION (DNm) that is subtracted
 *      from the driver's torque request inside MCM_calculateCommands.
 *   4. Any brake / significant steering / throttle-lift exits launch.
 *
 * NOTE: In MCM_calculateCommands, final torque never goes negative during
 *       active acceleration (see rule 9).
 * ------------------------------------------------------------------ */
void launchControlTorqueCalculation(LaunchControl *me, TorqueEncoder *tps, BrakePressureSensor *bps, MotorController *mcm)
{
    if (me == NULL || tps == NULL || bps == NULL || mcm == NULL) { return; }

    sbyte2 speedKph       = MCM_getGroundSpeedKPH(mcm);
    sbyte2 steeringAngle  = steering_degrees();
    sbyte2 mcm_TorqueMaxDNm = MCM_commands_getTorqueLimit(mcm);

    /* Arm launch control while stationary, brakes applied, button held */
    if (Sensor_LCButton.sensorValue == TRUE && speedKph < 5 && bps->percent < 0.35f) {
        me->LCReady = TRUE;
    }

    /* While armed & holding the button, force zero reduction and (re)tune PID */
    if (me->LCReady == TRUE && Sensor_LCButton.sensorValue == TRUE) {
        me->lcTorqueReductionDNm = 0;
        /* Tuning for torque-REDUCTION PID (DNm / slip-unit).
         * At 100% slip overshoot (error = 0.85 past the 0.15 target, saturated
         * to 1.0) with kp=2000 we'd command ~1700 DNm reduction -> saturated
         * by mcm_TorqueMaxDNm. Start conservative; tune on track. */
        initPIDController(me->pidController, 2000.0f, 500.0f, 50.0f);
    }

    /* Launch active: button released, throttle pinned, rolling */
    if (me->LCReady == TRUE && Sensor_LCButton.sensorValue == FALSE && tps->travelPercent > 0.90f) {
        me->LCStatus = TRUE;
        if (speedKph > 3) {
            /* dt = 10ms loop = 0.01s (hard real-time cadence) */
            Calctorque = calculatePIDReduction(me->pidController, me->targetSlip,
                                               me->slipRatio, 0.01f, mcm_TorqueMaxDNm);
            me->lcTorqueReductionDNm = (sbyte2)Calctorque;
        } else {
            /* Below 3 kph the slip signal is unreliable -> no reduction */
            me->lcTorqueReductionDNm = 0;
        }
    }

    /* Exit conditions: brake, steering angle > 35 deg, or throttle lift */
    if (bps->percent > 0.05f
        || steeringAngle >  35
        || steeringAngle < -35
        || (tps->travelPercent < 0.90f && me->LCStatus == TRUE))
    {
        me->LCStatus            = FALSE;
        me->LCReady             = FALSE;
        me->lcTorqueReductionDNm = 0;
    }

    /* Push the state & reduction to the motor controller. Reduction is in
     * DNm (MCM torque units: 100 = 10.0 Nm). */
    MCM_update_LaunchControl_State(mcm, me->LCStatus);
    MCM_update_LaunchControl_TorqueReductionDNm(mcm, me->lcTorqueReductionDNm);
}

bool getLaunchControlStatus(LaunchControl *me)
{
    return (me == NULL) ? FALSE : me->LCStatus;
}

sbyte2 getCalculatedTorque(void)
{
    return (sbyte2)Calctorque;
}

ubyte1 getButtonDebug(LaunchControl *me)
{
    return (me == NULL) ? 0 : me->buttonDebug;
}
