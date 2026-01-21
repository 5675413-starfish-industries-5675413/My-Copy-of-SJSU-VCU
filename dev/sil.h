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

#ifdef SIL_BUILD

// Output mode flags - can be combined using bitwise OR
#define SIL_OUTPUT_EFFICIENCY   0x01  // Output efficiency-related values
#define SIL_OUTPUT_POWERLIMIT   0x02  // Output power limit-related values
#define SIL_OUTPUT_MOTORCTRL    0x04  // Output motor controller values
#define SIL_OUTPUT_ALL          0x07  // Output all available values (EFFICIENCY | POWERLIMIT | MOTORCTRL)

// Compile-time output mode configuration
// Define this at compile time using -DSIL_OUTPUT_MODE_CONFIG=<mode>
// Examples:
//   -DSIL_OUTPUT_MODE_CONFIG=SIL_OUTPUT_EFFICIENCY
//   -DSIL_OUTPUT_MODE_CONFIG=SIL_OUTPUT_POWERLIMIT
//   -DSIL_OUTPUT_MODE_CONFIG="(SIL_OUTPUT_EFFICIENCY|SIL_OUTPUT_POWERLIMIT)"
//   -DSIL_OUTPUT_MODE_CONFIG=SIL_OUTPUT_ALL
// If not defined, defaults to SIL_OUTPUT_ALL
#ifndef SIL_OUTPUT_MODE_CONFIG
#define SIL_OUTPUT_MODE_CONFIG SIL_OUTPUT_ALL
#endif

/**
 * Set the output mode for JSON output
 * @param mode Bitmask of output flags (SIL_OUTPUT_EFFICIENCY, SIL_OUTPUT_POWERLIMIT, SIL_OUTPUT_MOTORCTRL, or SIL_OUTPUT_ALL)
 */
void sil_set_output_mode(ubyte1 mode);

/**
 * Get the current output mode
 * @return Current output mode bitmask
 */
ubyte1 sil_get_output_mode(void);

/**
 * Read initial JSON configuration from stdin during initialization
 * @param pl PowerLimit struct pointer (can be NULL if not needed)
 * @param mcm MotorController struct pointer (can be NULL if not needed)
 * @param tps TorqueEncoder struct pointer (can be NULL if not needed)
 * @return 0 on success, -1 on error
 */
int sil_read_initial_json(PowerLimit* pl, MotorController* mcm, TorqueEncoder* tps);

/**
 * Read JSON input from stdin in the main loop (non-blocking)
 * @param pl PowerLimit struct pointer (can be NULL if not needed)
 * @param mcm MotorController struct pointer (can be NULL if not needed)
 * @param tps TorqueEncoder struct pointer (can be NULL if not needed)
 * @return 0 on success, -1 on error, 1 if no data available (non-blocking)
 */
int sil_read_json_input(PowerLimit* pl, MotorController* mcm, TorqueEncoder* tps);

/**
 * Write JSON output to stdout based on configured output mode
 * Dynamically outputs only available data based on:
 * - Current output mode settings (compile-time and runtime)
 * - Available struct pointers (NULL checks)
 * - Field availability
 * 
 * @param mcm MotorController struct pointer (can be NULL)
 * @param pl PowerLimit struct pointer (can be NULL)
 * @param eff Efficiency struct pointer (can be NULL)
 */
void sil_write_json_output(MotorController* mcm, PowerLimit* pl, Efficiency* eff);

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

