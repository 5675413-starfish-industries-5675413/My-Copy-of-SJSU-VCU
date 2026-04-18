#ifndef _DERATING_H
#define _DERATING_H

#include "IO_Driver.h"
#include "motorController.h"

/*****************************************************************************
 * Software Thermal Derating
 *****************************************************************************
 * Monitors motor and inverter temperatures and smoothly scales down the
 * motor controller's commanded torque limit as temps climb toward the hard
 * thermal fault ceiling. Goal: finish the 22km Endurance event instead of
 * hitting a hard shutdown at the thermal fault threshold.
 *
 * Derating map (linear):
 *   T <= T_safe  (75 C) : 100% torque
 *   T_safe < T < T_max (90 C) : linear ramp 100% -> 30%
 *   T >= T_max  (90 C) :  30% torque
 *
 * The highest of (motor temp, inverter temp) drives the derating so the
 * hotter component wins. The scaled limit is written back to the MCM via
 * MCM_setMaxTorqueDNm(), so subsequent MCM_calculateCommands calls will
 * already see the reduced ceiling.
 ****************************************************************************/

#define DERATE_TEMP_SAFE_C      75   /* Below this: no derating */
#define DERATE_TEMP_MAX_C       90   /* At/above this: torque floored at DERATE_FLOOR */
#define DERATE_FLOOR            0.30f /* Minimum allowed torque fraction (30%) */

typedef struct _ThermalDerating
{
    ubyte2 baselineTorqueMaxDNm;  /* Original un-derated torque ceiling (DNm) */
    float4 currentMultiplier;     /* Last applied multiplier [DERATE_FLOOR..1.0] */
    sbyte2 highestTempC;          /* Last driving (max) temperature used */
} ThermalDerating;

ThermalDerating *ThermalDerating_new(ubyte2 baselineTorqueMaxDNm);

/* Compute derating from current motor/inverter temperatures and push the new
 * torque ceiling into the MCM. Safe to call every 10ms main-loop cycle. */
void ThermalDerating_update(ThermalDerating *me, MotorController *mcm);

float4 ThermalDerating_getMultiplier(ThermalDerating *me);
sbyte2 ThermalDerating_getHighestTempC(ThermalDerating *me);

#endif /* _DERATING_H */
