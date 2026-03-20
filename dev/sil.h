/*****************************************************************************
 * sil.h - Software-In-the-Loop (SIL) interface functions
 * Provides functions for JSON input/output in SIL testing mode
 *****************************************************************************/

#ifndef _SIL_H
#define _SIL_H

#include "IO_Driver.h"
#include "motorController.h"
#include "powerLimit.h"
#include "torqueEncoder.h"
#include "efficiency.h"
#include "bms.h"
#include "brakePressureSensor.h"
#include "canManager.h"
#include "cooling.h"
#include "drs.h"
#include "LaunchControl.h"
#include "PID.h"
#include "regen.h"
#include "wheelSpeeds.h"
#include "avlTree.h"
#include "brakePressureSensor.h"
#include "canManager.h"
#include "hashTable.h"
#include "instrumentCluster.h"
#include "readyToDriveSound.h"
#include "safety.h"
#include "sensors.h"
#include "serial.h"
#include "torqueEncoder.h"
#include "watchdog.h"
#include "diagnostics.h"

// regen, motorcontroller, powerlimit, efficiency, brakepressuresensor, launchcontrol, wheelspeeds, pid, torqueencoder, sensor, safetyChecker, readyToDriveSound

#ifdef SIL_BUILD

/**
 * Read initial JSON configuration from stdin during initialization
 * @param pl PowerLimit struct pointer (can be NULL if not needed)
 * @param mcm MotorController struct pointer (can be NULL if not needed)
 * @param tps TorqueEncoder struct pointer (can be NULL if not needed)
 * @param bps BrakePressureSensor struct pointer (can be NULL if not needed)
 * @param regen Regen struct pointer (can be NULL if not needed)
 * @return 0 on success, -1 on error
 */
int sil_read_initial_json(PowerLimit* pl, MotorController* mcm, TorqueEncoder* tps, BrakePressureSensor* bps, Regen* regen);

/**
 * Read JSON input from stdin in the main loop (non-blocking)
 * @param pl PowerLimit struct pointer (can be NULL if not needed)
 * @param mcm MotorController struct pointer (can be NULL if not needed)
 * @param tps TorqueEncoder struct pointer (can be NULL if not needed)
 * @param bps BrakePressureSensor struct pointer (can be NULL if not needed)
 * @param regen Regen struct pointer (can be NULL if not needed)
 * @return 0 on success, -1 on error, 1 if no data available (non-blocking)
 */
int sil_read_json_input(PowerLimit* pl, MotorController* mcm, TorqueEncoder* tps, BrakePressureSensor* bps, Regen* regen);

/**
 * Write JSON output to stdout.
 * Always outputs all available sections (efficiency, power_limit, motor_controller)
 * based on which struct pointers are non-NULL.
 * 
 * @param mcm MotorController struct pointer (can be NULL)
 * @param pl PowerLimit struct pointer (can be NULL)
 * @param eff Efficiency struct pointer (can be NULL)
 */
// void sil_write_json_output(MotorController* mcm, PowerLimit* pl, Efficiency* eff);
void sil_write_json_output(MotorController* mcm, PowerLimit* pl, Efficiency* eff, BatteryManagementSystem* bms, LaunchControl* lc, BrakePressureSensor* bps, PID* pid, Regen *regen, InstrumentCluster *ic, ReadyToDriveSound *rtds, SafetyChecker *sc, Sensor *sensor, TorqueEncoder *tps, WatchDog *wd, ubyte1 output_mode);

/**
 * Restore TPS values after TorqueEncoder_update overwrites them
 * This should be called after TorqueEncoder_update() in the main loop
 * @param tps TorqueEncoder struct pointer
 * @param saved_travelPercent Saved travel percent value
 * @param saved_tps0_percent Saved TPS0 percent value
 * @param saved_tps1_percent Saved TPS1 percent value
 */
void sil_restore_tps_values(TorqueEncoder* tps, float4* saved_travelPercent, 
                            float4* saved_tps0_percent, float4* saved_tps1_percent);

#endif // SIL_BUILD

#endif // _SIL_H

