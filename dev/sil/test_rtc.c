#define _POSIX_C_SOURCE 199309L  // enable clock_gettime/nanosleep on POSIX
#include "IO_RTC.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

/* tiny helper: sleep about N microseconds */
static void sleep_us(unsigned usec) {
    struct timespec req;
    req.tv_sec  = usec / 1000000u;
    req.tv_nsec = (long)(usec % 1000000u) * 1000L;
    nanosleep(&req, NULL);
}

/* dummy periodic handler */
static volatile int g_ticks = 0;
static void on_tick(void) { g_ticks++; }

/* assertions */
static void expect_eq(const char* name, int got, int want) {
    if (got != want) printf("[FAIL] %s: got=%d want=%d\n", name, got, want);
    assert(got == want);
}
static void expect_ge(const char* name, unsigned got, unsigned want_min) {
    if (got < want_min) printf("[FAIL] %s: got=%u want>=%u\n", name, got, want_min);
    assert(got >= want_min);
}

int main(void) {
    IO_ErrorType e;
    ubyte4 ts = 0;

    // ---- Before init: StartTime should report NOT_CONFIGURED
    e = IO_RTC_StartTime(&ts);
    expect_eq("StartTime before init -> NOT_CONFIGURED", (int)e, (int)IO_E_CHANNEL_NOT_CONFIGURED);

    // ---- Before init: GetTimeUS returns 0 by spec
    {
        ubyte4 dt = IO_RTC_GetTimeUS(123u);
        expect_eq("GetTimeUS before init -> 0", (int)dt, 0);
    }

    // ---- Init once -> OK; twice -> CHANNEL_BUSY
    e = IO_RTC_Init();
    expect_eq("Init first -> OK", (int)e, (int)IO_E_OK);

    e = IO_RTC_Init();
    expect_eq("Init again -> CHANNEL_BUSY", (int)e, (int)IO_E_CHANNEL_BUSY);

    // ---- StartTime: NULL pointer -> NULL_POINTER
    e = IO_RTC_StartTime(NULL);
    expect_eq("StartTime NULL -> NULL_POINTER", (int)e, (int)IO_E_NULL_POINTER);

    // ---- StartTime + GetTimeUS basic timing
    e = IO_RTC_StartTime(&ts);
    expect_eq("StartTime after init -> OK", (int)e, (int)IO_E_OK);

    sleep_us(3000);  // ~3 ms

    ubyte4 dt = IO_RTC_GetTimeUS(ts);
    // allow jitter; just verify it's positive and roughly >= 2000 us
    expect_ge("GetTimeUS >= ~2000us", (unsigned)dt, 2000u);

    // ---- PeriodicInit argument checks
    e = IO_RTC_PeriodicInit(0, on_tick);
    expect_eq("PeriodicInit period=0 -> INVALID_PARAMETER", (int)e, (int)IO_E_INVALID_PARAMETER);

    e = IO_RTC_PeriodicInit(1000, NULL);
    expect_eq("PeriodicInit handler=NULL -> NULL_POINTER", (int)e, (int)IO_E_NULL_POINTER);

    // ---- PeriodicInit OK then BUSY
    e = IO_RTC_PeriodicInit(1000, on_tick);
    expect_eq("PeriodicInit OK", (int)e, (int)IO_E_OK);

    e = IO_RTC_PeriodicInit(1000, on_tick);
    expect_eq("PeriodicInit again -> BUSY", (int)e, (int)IO_E_BUSY);

    // ---- PeriodicDeInit OK then NOT_CONFIGURED
    e = IO_RTC_PeriodicDeInit();
    expect_eq("PeriodicDeInit OK", (int)e, (int)IO_E_OK);

    e = IO_RTC_PeriodicDeInit();
    expect_eq("PeriodicDeInit again -> NOT_CONFIGURED", (int)e, (int)IO_E_PERIODIC_NOT_CONFIGURED);

    puts("All RTC tests passed.");
    return 0;
}
