/******************************************************************************
 * virtualSensors.c
 *
 * See virtualSensors.h for the theory-of-operation. This translation unit
 * provides the storage for the VS_* globals and the CAN-frame decoder.
 *
 * MISRA / SAFETY
 *   - All inputs are clamped to sane physical ranges before use.
 *   - NULL frame pointer is handled gracefully.
 *   - Frame length is validated before dereferencing data[].
 *   - No standard C library is used; only TTTech proprietary types.
 *****************************************************************************/
#include "virtualSensors.h"

/*----------------------------------------------------------------------------
 * Global state (external linkage -- declared in virtualSensors.h)
 *
 * Zero-initialized so that even if the bench rig is quiet the VCU sees a
 * safe, neutral input (no throttle, no brake, car stationary, wheel straight).
 *----------------------------------------------------------------------------*/
bool   VS_benchMode       = FALSE;
float4 VS_apps_percent    = 0.0f;
float4 VS_bps_percent     = 0.0f;
float4 VS_wss_front_mps   = 0.0f;
float4 VS_wss_rear_mps    = 0.0f;
sbyte2 VS_steering_deg    = 0;

/*----------------------------------------------------------------------------
 * VS_setBenchMode
 *   Latch the bench flag from the main-loop bench detector.
 *   Intentionally trivial -- kept as a function (not a macro / direct write)
 *   so a future revision can add logging or EEPROM-backed persistence without
 *   touching every call-site.
 *----------------------------------------------------------------------------*/
void VS_setBenchMode(bool on)
{
    VS_benchMode = on;
}

/*----------------------------------------------------------------------------
 * VS_parseOverrideCanMessage
 *   Decode a single 0x5FE frame (see virtualSensors.h for byte layout).
 *
 *   Receiver-side validation (MISRA-C):
 *     - frame == NULL         -> no-op
 *     - frame->length < 8     -> no-op (partial frames are ignored)
 *     - percent bytes > 100   -> clamped to 100
 *     - wheel speeds are unsigned (no reverse in bench today); we store m/s
 *----------------------------------------------------------------------------*/
void VS_parseOverrideCanMessage(const IO_CAN_DATA_FRAME *frame)
{
    if (frame == NULL)         { return; }
    if (frame->length < 8U)    { return; }

    /* ---- byte 0: APPS percent (0..100) -------------------------------- */
    ubyte1 appsRaw = frame->data[0];
    if (appsRaw > (ubyte1)100U) { appsRaw = (ubyte1)100U; }
    VS_apps_percent = (float4)appsRaw / 100.0f;

    /* ---- byte 1: BPS percent (0..100) --------------------------------- */
    ubyte1 bpsRaw = frame->data[1];
    if (bpsRaw > (ubyte1)100U) { bpsRaw = (ubyte1)100U; }
    VS_bps_percent = (float4)bpsRaw / 100.0f;

    /* ---- bytes 2,3: front wheels, kph*10, little-endian --------------- */
    ubyte2 frontRaw =  (ubyte2)frame->data[2]
                    | ((ubyte2)frame->data[3] << 8);
    float4 frontKph = (float4)frontRaw / 10.0f;
    VS_wss_front_mps = frontKph / 3.6f;          /* kph -> m/s */

    /* ---- bytes 4,5: rear wheels, kph*10, little-endian ---------------- */
    ubyte2 rearRaw  =  (ubyte2)frame->data[4]
                    | ((ubyte2)frame->data[5] << 8);
    float4 rearKph  = (float4)rearRaw / 10.0f;
    VS_wss_rear_mps = rearKph / 3.6f;

    /* ---- bytes 6,7: steering angle, signed 16-bit LE, degrees --------- */
    ubyte2 steerRaw = (ubyte2)frame->data[6]
                    | ((ubyte2)frame->data[7] << 8);
    VS_steering_deg = (sbyte2)steerRaw;
}
