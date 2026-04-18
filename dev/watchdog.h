#ifndef _WATCHDOG_H
#define _WATCHDOG_H

#include "IO_Driver.h"

/*****************************************************************************
 * Watchdog
 *****************************************************************************
 * HARDWARE-ONLY DESIGN.
 *
 * A software watchdog timer running on the same core as the main loop CANNOT
 * save a genuinely frozen / hung microcontroller -- if main.c is stuck in an
 * infinite loop, no software check ever runs. We rely exclusively on the
 * native TTTech HY-TTC 50 hardware watchdog, which is tickled automatically
 * by the IO_Driver_TaskBegin() / IO_Driver_TaskEnd() pair that wraps every
 * main loop iteration.
 *
 * If main.c hangs and the TaskBegin/End pair stops running within its
 * configured window, the HY-TTC 50 hardware will:
 *   1. Reset the microcontroller.
 *   2. Drop the safe-state outputs, physically cutting the tractive system
 *      via the shutdown circuit.
 *
 * All previous software-watchdog helpers (WatchDog_new / pet / reset / check)
 * have been removed. DO NOT re-introduce them -- they cannot deliver the
 * failure-mode guarantees required for FSAE EV / FMEA compliance.
 ****************************************************************************/

/* Intentionally empty: no software watchdog API is provided. */

#endif /* _WATCHDOG_H */
