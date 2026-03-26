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
 * @param lc LaunchControl struct pointer (can be NULL if not needed)
 * @param wss WheelSpeeds struct pointer (can be NULL if not needed)
 * @return 0 on success, -1 on error
 */
int SIL_read_config(PowerLimit* pl, MotorController* mcm, TorqueEncoder* tps, BrakePressureSensor* bps, Regen* regen, LaunchControl* lc, WheelSpeeds* wss, Efficiency* eff, BatteryManagementSystem* bms);

/**
 * Read JSON input from stdin in the main loop (non-blocking)
 * @param pl PowerLimit struct pointer (can be NULL if not needed)
 * @param mcm MotorController struct pointer (can be NULL if not needed)
 * @param tps TorqueEncoder struct pointer (can be NULL if not needed)
 * @param bps BrakePressureSensor struct pointer (can be NULL if not needed)
 * @param regen Regen struct pointer (can be NULL if not needed)
 * @param lc LaunchControl struct pointer (can be NULL if not needed)
 * @param wss WheelSpeeds struct pointer (can be NULL if not needed)
 * @return 0 on success, -1 on error, 1 if no data available (non-blocking)
 */
int SIL_read(PowerLimit* pl, MotorController* mcm, TorqueEncoder* tps, BrakePressureSensor* bps, Regen* regen, LaunchControl* lc, WheelSpeeds* wss, Efficiency* eff, BatteryManagementSystem* bms);

/**
 * Write JSON output to stdout.
 * Always outputs all available sections (efficiency, power_limit, motor_controller)
 * based on which struct pointers are non-NULL.
 * 
 * @param mcm MotorController struct pointer (can be NULL)
 * @param pl PowerLimit struct pointer (can be NULL)
 * @param eff Efficiency struct pointer (can be NULL)
 */
// void SIL_write(MotorController* mcm, PowerLimit* pl, Efficiency* eff);
void SIL_write(MotorController* mcm, PowerLimit* pl, Efficiency* eff, BatteryManagementSystem* bms, LaunchControl* lc, BrakePressureSensor* bps, Regen *regen, WheelSpeeds *wss, TorqueEncoder *tps);

/**
 * Restore cached SIL TPS values after TorqueEncoder_update overwrites them.
 * This should be called after TorqueEncoder_update() in the main loop.
 * @param tps TorqueEncoder struct pointer
 */
void SIL_restore_tps(TorqueEncoder* tps);

/**
 * Restore cached SIL BPS voltages after BrakePressureSensor_update overwrites them.
 * This should be called after BrakePressureSensor_update() in the main loop.
 * @param bps BrakePressureSensor struct pointer
 */
void SIL_restore_bps(BrakePressureSensor* bps);

#endif // SIL_BUILD

#endif // _SIL_H

