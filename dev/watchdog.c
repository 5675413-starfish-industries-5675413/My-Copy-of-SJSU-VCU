#include "watchdog.h"

/*****************************************************************************
 * Watchdog
 *****************************************************************************
 * Intentionally empty translation unit.
 *
 * All prior software-watchdog functions (WatchDog_new/_pet/_reset/_check)
 * have been REMOVED. The HY-TTC 50 hardware watchdog is the sole watchdog
 * mechanism for this VCU. It is automatically tickled by the
 * IO_Driver_TaskBegin() / IO_Driver_TaskEnd() pair wrapping the main loop
 * in main.c. If that pair ever fails to run within its window (main loop
 * hung), the hardware will reset the MCU and drop safe-state outputs,
 * physically opening the tractive-system shutdown circuit.
 *
 * See watchdog.h for the rationale.
 ****************************************************************************/
