#include "IO_WDTimer.h"
#include "IO_Constants.h"

typedef unsigned char ubyte1;
typedef unsigned long ubyte4;

// ---------------- Configuration ----------------

// Watchdog timer state
typedef struct {
    bool initialized;
    ubyte4 timeout_us;
    ubyte4 service_count;
    ubyte4 last_service_time;  // For future timing simulation
} WDTimer_State;

// Global state
static WDTimer_State wdtimer_state = {FALSE, 0, 0, 0};

// ---------------- Main Functions ----------------

/**
 * Initialization of the Watchdog Timer
 */
IO_ErrorType IO_WDTimer_Init(ubyte4 timeout)
{
    // Validate timeout range (3us to 13.4s)
    if (timeout < IO_WDTIMER_TIMEOUT_MIN || timeout > IO_WDTIMER_TIMEOUT_MAX)
    {
        // In SIL, we'll be lenient and just clamp the value
        if (timeout < IO_WDTIMER_TIMEOUT_MIN)
        {
            timeout = IO_WDTIMER_TIMEOUT_MIN;
        }
        if (timeout > IO_WDTIMER_TIMEOUT_MAX)
        {
            timeout = IO_WDTIMER_TIMEOUT_MAX;
        }
    }
    
    // Initialize watchdog
    wdtimer_state.initialized = TRUE;
    wdtimer_state.timeout_us = timeout;
    wdtimer_state.service_count = 0;
    wdtimer_state.last_service_time = 0;
    
    return IO_E_OK;
}

/**
 * Disable the Watchdog Timer
 */
IO_ErrorType IO_WDTimer_DeInit(void)
{
    // Disable watchdog
    wdtimer_state.initialized = FALSE;
    wdtimer_state.timeout_us = 0;
    wdtimer_state.service_count = 0;
    wdtimer_state.last_service_time = 0;
    
    return IO_E_OK;
}

/**
 * Service the Watchdog timer
 */
IO_ErrorType IO_WDTimer_Service(void)
{
    // In SIL, we just track that the watchdog was serviced
    // In real hardware, this would reset the watchdog counter
    
    if (wdtimer_state.initialized)
    {
        wdtimer_state.service_count++;
        // Update last service time (could use RTC in future for actual timing)
    }
    
    // Always return OK in SIL (no timeout simulation by default)
    return IO_E_OK;
}

// ---------------- Helper Functions for Testing ----------------

/**
 * SIL-specific function to check if watchdog is initialized
 * This function is not part of the real IO_WDTimer API
 */
bool IO_WDTimer_SIL_IsInitialized(void)
{
    return wdtimer_state.initialized;
}

/**
 * SIL-specific function to get configured timeout
 * This function is not part of the real IO_WDTimer API
 */
ubyte4 IO_WDTimer_SIL_GetTimeout(void)
{
    return wdtimer_state.timeout_us;
}

/**
 * SIL-specific function to get service count
 * This function is not part of the real IO_WDTimer API
 */
ubyte4 IO_WDTimer_SIL_GetServiceCount(void)
{
    return wdtimer_state.service_count;
}

/**
 * SIL-specific function to reset service count
 * This function is not part of the real IO_WDTimer API
 */
void IO_WDTimer_SIL_ResetServiceCount(void)
{
    wdtimer_state.service_count = 0;
}

/**
 * SIL-specific function to check if watchdog should have timed out
 * This function is not part of the real IO_WDTimer API
 * 
 * In a more sophisticated SIL, this could be called periodically to check
 * if the watchdog hasn't been serviced within the timeout period.
 * For now, it's a placeholder for future timing simulation.
 */
bool IO_WDTimer_SIL_WouldTimeout(ubyte4 elapsed_time_us)
{
    if (!wdtimer_state.initialized)
    {
        return FALSE;
    }
    
    // Check if elapsed time exceeds configured timeout
    return (elapsed_time_us > wdtimer_state.timeout_us);
}

