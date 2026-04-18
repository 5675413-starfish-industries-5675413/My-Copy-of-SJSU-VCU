/******************************************************************************
 * virtualSensors.h
 *
 * Virtual HIL (Hardware-In-the-Loop) sensor overrides.
 *
 * PURPOSE
 *   When the VCU boots with IO_DI_06 pulled LOW ("bench mode", detected in
 *   main.c), the sensor modules (TPS/APPS, BPS, WSS, SAS) bypass their
 *   IO_ADC_Get / IO_PWD_Get hardware reads and instead consume the Virtual
 *   Sensor values defined here. These values are updated in real-time by a
 *   single inbound CAN frame (ID 0x5FE) transmitted by the bench simulator.
 *
 *   This lets us exercise Traction Control, Regen, DRS, the 80 kW ceiling,
 *   and the APPS implausibility debounce from a PC with nothing more than a
 *   CAN dongle -- no HIL box required.
 *
 * SAFETY
 *   The bench flag is latched ONCE at boot (main.c reads IO_DI_06 for ~55ms
 *   then deinits the pin). A car in the field has IO_DI_06 pulled HIGH by
 *   default, so this subsystem is a no-op on the real vehicle.
 *
 * CAN FRAME LAYOUT (ID 0x5FE, STD frame, 8 data bytes)
 *   byte 0     APPS percent              ubyte1, 0..100        -> /100 float
 *   byte 1    BPS  percent              ubyte1, 0..100        -> /100 float
 *   byte 2,3   Front wheel speed * 10    ubyte2 LE, [kph*10]   -> 0.1 kph res
 *   byte 4,5   Rear  wheel speed * 10    ubyte2 LE, [kph*10]
 *   byte 6,7   Steering angle            sbyte2 LE, [degrees]  +/- 32767
 *
 * All types conform to .cursorrules (ubyte1/ubyte2/sbyte2/float4/bool).
 *****************************************************************************/
#ifndef _VIRTUAL_SENSORS_H
#define _VIRTUAL_SENSORS_H

#include "IO_Driver.h"
#include "IO_CAN.h"

/* CAN ID the bench rig uses to push APPS/BPS/WSS/SAS into the VCU. */
#define BENCH_SENSOR_OVERRIDE_CAN_ID  ((ubyte2)0x5FE)

/*----------------------------------------------------------------------------
 * Global state (read by the sensor modules when VS_benchMode == TRUE)
 *----------------------------------------------------------------------------*/
extern bool   VS_benchMode;         /* set once in main.c after IO_DI_06 read */

extern float4 VS_apps_percent;      /* 0.0f .. 1.0f */
extern float4 VS_bps_percent;       /* 0.0f .. 1.0f */
extern float4 VS_wss_front_mps;     /* front wheels ground speed, m/s */
extern float4 VS_wss_rear_mps;      /* rear  wheels ground speed, m/s */
extern sbyte2 VS_steering_deg;      /* steering angle, degrees */

/*----------------------------------------------------------------------------
 * API
 *----------------------------------------------------------------------------*/
/* Latch the bench flag. Call once from main.c with the IO_DI_06 result. */
void VS_setBenchMode(bool on);

/* Decode an 0x5FE frame and update the VS_* globals. Ignored if NULL or
 * if the frame is shorter than the expected 8 bytes. */
void VS_parseOverrideCanMessage(const IO_CAN_DATA_FRAME *frame);

#endif /* _VIRTUAL_SENSORS_H */
