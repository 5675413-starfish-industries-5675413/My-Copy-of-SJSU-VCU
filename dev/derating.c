#include <stdlib.h>
#include "IO_Driver.h"
#include "IO_RTC.h"

#include "derating.h"
#include "mathFunctions.h"
#include "motorController.h"

/*****************************************************************************
 * Thermal Derating
 *****************************************************************************
 * Linear ramp from 100% torque (at DERATE_TEMP_SAFE_C) down to DERATE_FLOOR
 * (at DERATE_TEMP_MAX_C). The hotter of the motor/inverter temps drives it.
 * Keeping a small amount of torque (30%) available above the ceiling lets
 * the driver still crawl the car off the track without a hard fault.
 ****************************************************************************/

ThermalDerating *ThermalDerating_new(ubyte2 baselineTorqueMaxDNm)
{
    ThermalDerating *me = (ThermalDerating *)malloc(sizeof(struct _ThermalDerating));
    if (me == NULL) { return NULL; } /* MISRA null guard */

    me->baselineTorqueMaxDNm = baselineTorqueMaxDNm;
    me->currentMultiplier    = 1.0f;
    me->highestTempC         = 0;
    return me;
}

void ThermalDerating_update(ThermalDerating *me, MotorController *mcm)
{
    if (me == NULL || mcm == NULL) { return; } /* MISRA null guard */

    sbyte2 motorTempC    = MCM_getMotorTemp(mcm);
    sbyte2 inverterTempC = MCM_getTemp(mcm); /* MCM control-board temp */

    /* Pick the hotter of the two so the most-stressed component limits us */
    sbyte2 hottest = (motorTempC > inverterTempC) ? motorTempC : inverterTempC;
    me->highestTempC = hottest;

    /* Compute the derating multiplier (linear ramp) */
    float4 multiplier = 1.0f;
    if (hottest <= DERATE_TEMP_SAFE_C) {
        multiplier = 1.0f;
    } else if (hottest >= DERATE_TEMP_MAX_C) {
        multiplier = DERATE_FLOOR;
    } else {
        /* Linear interpolation between (SAFE, 1.0) and (MAX, FLOOR) */
        float4 fractionHot = getPercent((float4)hottest,
                                        (float4)DERATE_TEMP_SAFE_C,
                                        (float4)DERATE_TEMP_MAX_C,
                                        TRUE);
        multiplier = 1.0f - (fractionHot * (1.0f - DERATE_FLOOR));
    }

    me->currentMultiplier = multiplier;

    /* Apply the derating to the MCM torque ceiling. Subsequent calls to
     * MCM_calculateCommands will use this reduced torqueMaximumDNm as the
     * 100%-throttle cap. */
    float4 newCeiling = (float4)me->baselineTorqueMaxDNm * multiplier;
    MCM_setMaxTorqueDNm(mcm, (ubyte2)newCeiling);
}

float4 ThermalDerating_getMultiplier(ThermalDerating *me)
{
    return (me == NULL) ? 1.0f : me->currentMultiplier;
}

sbyte2 ThermalDerating_getHighestTempC(ThermalDerating *me)
{
    return (me == NULL) ? 0 : me->highestTempC;
}
