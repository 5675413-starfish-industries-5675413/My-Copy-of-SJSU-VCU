#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>  // for malloc
#include "IO_Driver.h"

/* --------------------------------------------------------------------------
   Basic typedefs and constants (match your project)
   -------------------------------------------------------------------------- */
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* --------------------------------------------------------------------------
   Minimal structs used as inputs
   -------------------------------------------------------------------------- */
typedef struct _MotorController MotorController;
typedef struct {
    bool  calibrated;
    float travelPercent; // 0..1
} TorqueEncoder;

typedef struct {
    bool  calibrated;
    float percent; // 0..1
} BrakePressureSensor;

typedef struct {
    bool calibrated;
    float sensorValue;
} Sensor;

typedef struct {
    int dummy;
} ReadyToDriveSound;

/* --------------------------------------------------------------------------
   External APIs from your real modules
   -------------------------------------------------------------------------- */
MotorController *MotorController_new(void *sm, ubyte2 canMessageBaseID,
                                     ubyte1 initialDirection, sbyte2 torqueMaxDNm,
                                     sbyte1 minRegenSpeedKPH, sbyte1 regenRampStart);
void MCM_calculateCommands(MotorController *me, TorqueEncoder *tps, BrakePressureSensor *bps);
sbyte2 MCM_commands_getAppsTorque(MotorController *me);
sbyte4 MCM_getMotorRPM(MotorController *me);
sbyte4 MCM_getDCVoltage(MotorController *me);
sbyte4 MCM_getDCCurrent(MotorController *me);
sbyte2 MCM_get_PL_torqueCommand(MotorController *me);
bool   MCM_get_PL_state(MotorController *me);
void   MCM_incrementVoltageForTesting(MotorController* me, sbyte4 inc);
void   MCM_incrementCurrentForTesting(MotorController* me, sbyte4 inc);
void   MCM_incrementRPMForTesting(MotorController* me, sbyte4 inc);

typedef struct _PowerLimit PowerLimit;
PowerLimit* POWERLIMIT_new(bool plToggle);
void PowerLimit_calculateCommand(PowerLimit *me, MotorController *mcm, TorqueEncoder *tps);

/* --------------------------------------------------------------------------
   Implement missing globals & platform functions (replaces test_shims.c)
   -------------------------------------------------------------------------- */
Sensor Sensor_RTDButton = { .calibrated = TRUE, .sensorValue = TRUE };

void RTDS_setVolume(ReadyToDriveSound* r, int vol, ubyte4 dur_us) {
    (void)r; (void)vol; (void)dur_us;
    // no-op in SIL
}

void SerialManager_send(void* sm, const char* s) { (void)sm; (void)s; }
void Light_set(int light, float percent) { (void)light; (void)percent; }
void IO_DO_Set(int line, bool val) { (void)line; (void)val; }
void IO_RTC_StartTime(ubyte4* t) { if (t) *t = 0; }
ubyte4 IO_RTC_GetTimeUS(ubyte4 t0) { (void)t0; return 2*1000000; } // pretend >2s elapsed

/* --------------------------------------------------------------------------
   Rotary switch stub for PL modes
   -------------------------------------------------------------------------- */
typedef enum { PL_MODE_OFF=0, PL_MODE_30, PL_MODE_40, PL_MODE_50, PL_MODE_60, PL_MODE_80 } PLMode;
static PLMode g_plMode = PL_MODE_30;
PLMode getPLMode(void) { return g_plMode; }

/* --------------------------------------------------------------------------
   Torque encoder helper (mimics real module)
   -------------------------------------------------------------------------- */
void TorqueEncoder_getOutputPercent(TorqueEncoder *tps, float *out) {
    if (!tps || !out) return;
    float p = tps->travelPercent;
    if (p < 0) p = 0; if (p > 1) p = 1;
    *out = p;
}

/* --------------------------------------------------------------------------
   Simple closed-loop “plant” model
   -------------------------------------------------------------------------- */
static void plant_step(MotorController* mcm, sbyte2 plTorqueDNm, float rpm_target,
                       float vdc_target, float eff)
{
    MCM_incrementRPMForTesting(mcm, (sbyte4)(rpm_target - MCM_getMotorRPM(mcm)));
    MCM_incrementVoltageForTesting(mcm, (sbyte4)(vdc_target - MCM_getDCVoltage(mcm)));

    float T_nm = plTorqueDNm / 10.0f;
    float Pmech_kW = (T_nm * rpm_target) / 9549.0f;
    if (Pmech_kW < 0) Pmech_kW = 0.0f;
    float Pelec_kW = (eff > 0.01f) ? (Pmech_kW / eff) : Pmech_kW;
    float Idc = (vdc_target > 1.0f) ? ((Pelec_kW * 1000.0f) / vdc_target) : 0.0f;
    if (Idc < 0) Idc = 0;

    MCM_incrementCurrentForTesting(mcm, (sbyte4)(Idc - MCM_getDCCurrent(mcm)));
}

/* --------------------------------------------------------------------------
   Helper to apply a “driver torque request”
   -------------------------------------------------------------------------- */
static void set_driver_request(MotorController* mcm, TorqueEncoder* tps, BrakePressureSensor* bps,
                               float tps_pct_0to1)
{
    tps->calibrated = TRUE;
    tps->travelPercent = tps_pct_0to1;
    bps->calibrated = TRUE;
    bps->percent = 0.0f; // not braking
    MCM_calculateCommands(mcm, tps, bps);
}

/* --------------------------------------------------------------------------
   Main closed-loop test
   -------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------
   Main: single-shot test at commandedTorque = 100 dNm
   -------------------------------------------------------------------------- */
int main(void)
{
    // --- knobs you can tweak for the test ---
    const sbyte2 CMD_TORQUE_DNm = 1000;   // <-- your requested commanded torque (dNm)
    const float  VDC            = 400.0f;
    const float  RPM            = 3500.0f;
    const float  EFF            = 0.92f; // mech->elec efficiency model
    g_plMode = PL_MODE_30;               // PL target = 30 kW

    // --- create real components ---
    MotorController* mcm = MotorController_new(NULL, 0x500, 1, 2310, 5, 15);
    PowerLimit*      pl  = POWERLIMIT_new(TRUE);
    TorqueEncoder    tps = { .calibrated = TRUE, .travelPercent = 0.0f };
    BrakePressureSensor bps = { .calibrated = TRUE, .percent = 0.0f };

    // compute TPS % that yields appsTorque = CMD_TORQUE_DNm
    // appsTorque = torqueMaxDNm * tps_percent  => tps_percent = CMD / torqueMax
    const float tps_pct = (float)CMD_TORQUE_DNm / 2310.0f;

    // 1) set driver request (fills appsTorque inside MCM)
    set_driver_request(mcm, &tps, &bps, tps_pct);

    // read back driver-request torque
    sbyte2 appsDNm = MCM_commands_getAppsTorque(mcm);

    // 2) establish electrical state once so PL sees power:
    //    use the *driver* torque (not PL) to generate I = P/V (chicken-egg break)
    plant_step(mcm, appsDNm, RPM, VDC, EFF);

    // log the pre-PL power
    float preP_kW = (MCM_getDCVoltage(mcm) * MCM_getDCCurrent(mcm)) / 1000.0f;

    // 3) run PL exactly once at this operating point
    PowerLimit_calculateCommand(pl, mcm, &tps);

    // results
    sbyte2 plDNm   = MCM_get_PL_torqueCommand(mcm);
    bool   plOn    = MCM_get_PL_state(mcm);
    float  postP_kW = (MCM_getDCVoltage(mcm) * MCM_getDCCurrent(mcm)) / 1000.0f;

    printf("Fixed test @ CMD=%d dNm (TPS=%.2f%%), Vdc=%.0fV, RPM=%.0f\n",
           CMD_TORQUE_DNm, tps_pct*100.0f, VDC, RPM);
    printf("Pre-PL power:  %.2f kW\n", preP_kW);
    printf("PL status:     %s\n", plOn ? "ON" : "OFF");
    printf("PL torque cmd: %d dNm\n", plDNm);
    printf("Post-PL power: %.2f kW (unchanged here; plant not advanced after PL)\n", postP_kW);

    return 0;
}
