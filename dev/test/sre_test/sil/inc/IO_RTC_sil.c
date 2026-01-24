#include "IO_Driver.h"
#include "IO_RTC.h"
#include <time.h>

// ---------------- Configuration ----------------
/*      Initializes the RTC clock to a f_sys / 80.
 *      For a system clock of 80MHz the RTC resolution is 1us
 */
#define IO_RTC_TICK_US 1u

static bool initialized_RTC = FALSE;
static bool initialized_PERIODIC = FALSE;
static unsigned long long epoch_us = 0;

// ---------------- Helper Functions----------------
unsigned long long now_us(void)
{
    struct timespec ts;                                                                             // { long tv_sec; long tv_nsec; }
    clock_gettime(CLOCK_MONOTONIC, &ts);                                                            // time since an arbitrary start, not wall-clock
    return (unsigned long long)ts.tv_sec * 1000000ULL + (unsigned long long)(ts.tv_nsec / 1000ULL); // sec→µs + nsec→µs
}

// ---------------- Main Functions----------------
/* Initializes the RTC clock */
IO_ErrorType IO_RTC_Init(void)
{
    if (initialized_RTC)
    {
        return IO_E_CHANNEL_BUSY;
    }
    initialized_RTC = TRUE;
    epoch_us = now_us();
    return IO_E_OK;
}

/* Returns an RTC timestamp */
IO_ErrorType IO_RTC_StartTime(ubyte4 *const timestamp)
{
    if (timestamp == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    if (!initialized_RTC)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    *timestamp = (ubyte4)(ts.tv_sec * 1000000 + ts.tv_nsec / 1000);

    return IO_E_OK;
}

/* Returns the passed time in microseconds since the given timestamp */
ubyte4 IO_RTC_GetTimeUS(ubyte4 timestamp)
{
    if (!initialized_RTC)
    {
        return (ubyte4)0;
    }

    // current ticks since init; with IO_RTC_TICK_US==1 this is just microseconds
    unsigned long long now = now_us();
    ubyte4 current = (ubyte4)((now - epoch_us) / IO_RTC_TICK_US);

    // Unsigned 32-bit subtraction handles wrap-around naturally.
    // Valid as long as elapsed < ~71.6 minutes.
    return (ubyte4)(current - timestamp);
}

/* Initializes the Periodic timer */
IO_ErrorType IO_RTC_PeriodicInit(ubyte2 period, rtc_eventhandler_ptr event_handler)
{
    if (initialized_PERIODIC)
    {
        return IO_E_BUSY;
    }
    if (event_handler == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    if (period == 0 || period > 65535u)
    {
        return IO_E_INVALID_PARAMETER;
    }

    initialized_PERIODIC = TRUE;
    return IO_E_OK; // placeholder
}

/* Deinitializes the Periodic Timer */
IO_ErrorType IO_RTC_PeriodicDeInit(void)
{
    if (!initialized_PERIODIC) {
        return IO_E_PERIODIC_NOT_CONFIGURED;
    }

    initialized_PERIODIC = FALSE;
    return IO_E_OK;
}