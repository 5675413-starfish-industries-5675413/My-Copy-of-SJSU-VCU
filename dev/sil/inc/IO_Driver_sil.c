/**************************************************************************
 * SIL (Software-In-the-Loop) version of IO_Driver functions
 * Stubs for IO_Driver functions used in main.c
 **************************************************************************/

#include "IO_Driver.h"
#include "IO_Constants.h"

/**
 * Global initialization of IO driver
 * SIL stub implementation
 */
IO_ErrorType IO_Driver_Init(const IO_DRIVER_SAFETY_CONF * const safety_conf)
{
    // SIL stub - no actual hardware initialization needed
    (void)safety_conf; // Suppress unused parameter warning
    return IO_E_OK;
}

/**
 * Task function for IO Driver - called at the beginning of the task
 * SIL stub implementation
 */
IO_ErrorType IO_Driver_TaskBegin(void)
{
    // SIL stub - no actual task begin processing needed
    return IO_E_OK;
}

/**
 * Task function for IO Driver - called at the end of the task
 * SIL stub implementation
 */
IO_ErrorType IO_Driver_TaskEnd(void)
{
    // SIL stub - no actual task end processing needed
    return IO_E_OK;
}

