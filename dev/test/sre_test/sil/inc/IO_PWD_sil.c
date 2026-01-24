#include "IO_PWD.h"
#include "IO_Constants.h"
#include <string.h>

typedef unsigned char ubyte1;
typedef unsigned int ubyte2;
typedef unsigned long ubyte4;

// ---------------- Configuration ----------------

#define MAX_SIMPLE_PWD_CHANNELS 8   // IO_PWD_00..07
#define MAX_COMPLEX_PWD_CHANNELS 4  // IO_PWD_08..11

// Channel modes
typedef enum {
    PWD_MODE_NONE = 0,
    PWD_MODE_FREQ,
    PWD_MODE_PULSE,
    PWD_MODE_COMPLEX,
    PWD_MODE_INCREMENTAL,
    PWD_MODE_COUNTER
} PWD_Mode;

// Simple PWD channel state (IO_PWD_00..07)
typedef struct {
    bool initialized;
    PWD_Mode mode;
    ubyte1 freq_mode;     // IO_PWD_RISING_VAR or IO_PWD_FALLING_VAR
    ubyte1 pulse_mode;    // IO_PWD_HIGH_TIME or IO_PWD_LOW_TIME
    ubyte2 simulated_freq;     // Simulated frequency in Hz
    ubyte4 simulated_pulse;    // Simulated pulse width in us
} Simple_PWD_State;

// Complex PWD channel state (IO_PWD_08..11)
typedef struct {
    bool initialized;
    PWD_Mode mode;
    ubyte1 pulse_mode;
    ubyte1 freq_mode;
    ubyte1 timer_res;
    ubyte1 capture_count;
    ubyte1 threshold;
    ubyte1 pupd;
    
    // For incremental/counter modes
    ubyte1 inc_mode;
    ubyte1 count_direction;
    ubyte2 counter_value;
    ubyte1 neighbor_channel;    // For incremental mode (pairs channels)
    
    // Simulated values
    ubyte2 simulated_freq;
    ubyte4 simulated_pulse;
} Complex_PWD_State;

// Global state
static Simple_PWD_State simple_pwd_states[MAX_SIMPLE_PWD_CHANNELS];
static Complex_PWD_State complex_pwd_states[MAX_COMPLEX_PWD_CHANNELS];

// ---------------- Helper Functions ----------------

// Convert simple PWD channel to index
static int simple_pwd_to_index(ubyte1 channel)
{
    if (channel >= IO_PWD_00 && channel <= IO_PWD_07)
    {
        return channel - IO_PWD_00;
    }
    return -1;
}

// Convert complex PWD channel to index
static int complex_pwd_to_index(ubyte1 channel)
{
    if (channel >= IO_PWD_08 && channel <= IO_PWD_11)
    {
        return channel - IO_PWD_08;
    }
    return -1;
}

// Check if simple channel is initialized
static bool is_simple_pwd_initialized(ubyte1 channel)
{
    int idx = simple_pwd_to_index(channel);
    if (idx < 0 || idx >= MAX_SIMPLE_PWD_CHANNELS) return FALSE;
    return simple_pwd_states[idx].initialized;
}

// Check if complex channel is initialized
static bool is_complex_pwd_initialized(ubyte1 channel)
{
    int idx = complex_pwd_to_index(channel);
    if (idx < 0 || idx >= MAX_COMPLEX_PWD_CHANNELS) return FALSE;
    return complex_pwd_states[idx].initialized;
}

// Get incremental neighbor channel
static ubyte1 get_inc_neighbor(ubyte1 channel)
{
    // IO_PWD_08 <-> IO_PWD_09
    if (channel == IO_PWD_08) return IO_PWD_09;
    if (channel == IO_PWD_09) return IO_PWD_08;
    
    // IO_PWD_10 <-> IO_PWD_11
    if (channel == IO_PWD_10) return IO_PWD_11;
    if (channel == IO_PWD_11) return IO_PWD_10;
    
    return 0xFF;
}

// ---------------- Simple PWD Functions (IO_PWD_00..07) ----------------

/**
 * Setup frequency measurement
 */
IO_ErrorType IO_PWD_FreqInit(ubyte1 timer_channel, ubyte1 freq_mode)
{
    int idx = simple_pwd_to_index(timer_channel);
    
    if (idx < 0)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    if (simple_pwd_states[idx].initialized)
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    if (freq_mode != IO_PWD_RISING_VAR && freq_mode != IO_PWD_FALLING_VAR)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    simple_pwd_states[idx].initialized = TRUE;
    simple_pwd_states[idx].mode = PWD_MODE_FREQ;
    simple_pwd_states[idx].freq_mode = freq_mode;
    simple_pwd_states[idx].simulated_freq = 1000;  // Default 1kHz
    
    return IO_E_OK;
}

/**
 * Setup pulse width measurement
 */
IO_ErrorType IO_PWD_PulseInit(ubyte1 timer_channel, ubyte1 pulse_mode)
{
    int idx = simple_pwd_to_index(timer_channel);
    
    if (idx < 0)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    if (simple_pwd_states[idx].initialized)
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    if (pulse_mode != IO_PWD_HIGH_TIME && pulse_mode != IO_PWD_LOW_TIME)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    simple_pwd_states[idx].initialized = TRUE;
    simple_pwd_states[idx].mode = PWD_MODE_PULSE;
    simple_pwd_states[idx].pulse_mode = pulse_mode;
    simple_pwd_states[idx].simulated_pulse = 500;  // Default 500us
    
    return IO_E_OK;
}

/**
 * Get frequency
 */
IO_ErrorType IO_PWD_FreqGet(ubyte1 timer_channel, ubyte2 * const frequency)
{
    if (frequency == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    int idx = simple_pwd_to_index(timer_channel);
    
    if (idx < 0)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    if (!simple_pwd_states[idx].initialized || simple_pwd_states[idx].mode != PWD_MODE_FREQ)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    *frequency = simple_pwd_states[idx].simulated_freq;
    
    return IO_E_OK;
}

/**
 * Get pulse width
 */
IO_ErrorType IO_PWD_PulseGet(ubyte1 timer_channel, ubyte4 * const pulse_width)
{
    if (pulse_width == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    int idx = simple_pwd_to_index(timer_channel);
    
    if (idx < 0)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    if (!simple_pwd_states[idx].initialized || simple_pwd_states[idx].mode != PWD_MODE_PULSE)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    *pulse_width = simple_pwd_states[idx].simulated_pulse;
    
    return IO_E_OK;
}

/**
 * Deinitialize frequency measurement
 */
IO_ErrorType IO_PWD_FreqDeInit(ubyte1 timer_channel)
{
    int idx = simple_pwd_to_index(timer_channel);
    
    if (idx < 0)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    if (!simple_pwd_states[idx].initialized || simple_pwd_states[idx].mode != PWD_MODE_FREQ)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    memset(&simple_pwd_states[idx], 0, sizeof(Simple_PWD_State));
    
    return IO_E_OK;
}

/**
 * Deinitialize pulse width measurement
 */
IO_ErrorType IO_PWD_PulseDeInit(ubyte1 timer_channel)
{
    int idx = simple_pwd_to_index(timer_channel);
    
    if (idx < 0)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    if (!simple_pwd_states[idx].initialized || simple_pwd_states[idx].mode != PWD_MODE_PULSE)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    memset(&simple_pwd_states[idx], 0, sizeof(Simple_PWD_State));
    
    return IO_E_OK;
}

// ---------------- Complex PWD Functions (IO_PWD_08..11) ----------------

/**
 * Setup complex timer (frequency + pulse width)
 */
IO_ErrorType IO_PWD_ComplexInit(ubyte1 timer_channel, ubyte1 pulse_mode, ubyte1 freq_mode,
                               ubyte1 timer_res, ubyte1 capture_count, ubyte1 threshold,
                               ubyte1 pupd, IO_PWD_CPLX_SAFETY_CONF const * const safety_conf)
{
    int idx = complex_pwd_to_index(timer_channel);
    
    if (idx < 0)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    if (complex_pwd_states[idx].initialized)
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    // Validate parameters
    if (pulse_mode != IO_PWD_HIGH_TIME && pulse_mode != IO_PWD_LOW_TIME && pulse_mode != IO_PWD_PERIOD_TIME)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    if (freq_mode != IO_PWD_RISING_VAR && freq_mode != IO_PWD_FALLING_VAR)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    if (capture_count > 8)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    complex_pwd_states[idx].initialized = TRUE;
    complex_pwd_states[idx].mode = PWD_MODE_COMPLEX;
    complex_pwd_states[idx].pulse_mode = pulse_mode;
    complex_pwd_states[idx].freq_mode = freq_mode;
    complex_pwd_states[idx].timer_res = timer_res;
    complex_pwd_states[idx].capture_count = capture_count;
    complex_pwd_states[idx].threshold = threshold;
    complex_pwd_states[idx].pupd = pupd;
    complex_pwd_states[idx].simulated_freq = 1000;  // Default 1kHz
    complex_pwd_states[idx].simulated_pulse = 500;  // Default 500us
    
    return IO_E_OK;
}

/**
 * Get complex timer values
 */
IO_ErrorType IO_PWD_ComplexGet(ubyte1 timer_channel, ubyte2 * const frequency,
                              ubyte4 * const pulse_width, IO_PWD_PULSE_SAMPLES * const pulse_samples)
{
    if (frequency == NULL || pulse_width == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    int idx = complex_pwd_to_index(timer_channel);
    
    if (idx < 0)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    if (!complex_pwd_states[idx].initialized || complex_pwd_states[idx].mode != PWD_MODE_COMPLEX)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    *frequency = complex_pwd_states[idx].simulated_freq;
    *pulse_width = complex_pwd_states[idx].simulated_pulse;
    
    // Fill pulse samples if requested
    if (pulse_samples != NULL)
    {
        pulse_samples->pulse_samples_count = 1;
        pulse_samples->pulse_sample[0] = *pulse_width;
    }
    
    return IO_E_OK;
}

/**
 * Deinitialize complex timer
 */
IO_ErrorType IO_PWD_ComplexDeInit(ubyte1 timer_channel)
{
    int idx = complex_pwd_to_index(timer_channel);
    
    if (idx < 0)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    if (!complex_pwd_states[idx].initialized || complex_pwd_states[idx].mode != PWD_MODE_COMPLEX)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    memset(&complex_pwd_states[idx], 0, sizeof(Complex_PWD_State));
    
    return IO_E_OK;
}

// ---------------- Incremental Encoder Functions ----------------

/**
 * Setup incremental encoder
 */
IO_ErrorType IO_PWD_IncInit(ubyte1 inc_channel, ubyte1 mode, ubyte2 count_init,
                           ubyte1 threshold, ubyte1 pupd, IO_PWD_INC_SAFETY_CONF const * const safety_conf)
{
    int idx = complex_pwd_to_index(inc_channel);
    
    if (idx < 0)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    if (complex_pwd_states[idx].initialized)
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    // Validate mode
    if (mode != IO_PWD_INC_1_COUNT && mode != IO_PWD_INC_2_COUNT)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    // Get neighbor channel and check if available
    ubyte1 neighbor = get_inc_neighbor(inc_channel);
    int neighbor_idx = complex_pwd_to_index(neighbor);
    
    if (neighbor_idx >= 0 && complex_pwd_states[neighbor_idx].initialized)
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    // Initialize primary channel
    complex_pwd_states[idx].initialized = TRUE;
    complex_pwd_states[idx].mode = PWD_MODE_INCREMENTAL;
    complex_pwd_states[idx].inc_mode = mode;
    complex_pwd_states[idx].counter_value = count_init;
    complex_pwd_states[idx].threshold = threshold;
    complex_pwd_states[idx].pupd = pupd;
    complex_pwd_states[idx].neighbor_channel = neighbor;
    
    // Reserve neighbor channel
    if (neighbor_idx >= 0)
    {
        complex_pwd_states[neighbor_idx].initialized = TRUE;
        complex_pwd_states[neighbor_idx].mode = PWD_MODE_INCREMENTAL;
        complex_pwd_states[neighbor_idx].neighbor_channel = inc_channel;
    }
    
    return IO_E_OK;
}

/**
 * Get incremental counter value
 */
IO_ErrorType IO_PWD_IncGet(ubyte1 inc_channel, ubyte2 * const count)
{
    if (count == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    int idx = complex_pwd_to_index(inc_channel);
    
    if (idx < 0)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    if (!complex_pwd_states[idx].initialized || complex_pwd_states[idx].mode != PWD_MODE_INCREMENTAL)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    *count = complex_pwd_states[idx].counter_value;
    
    return IO_E_OK;
}

/**
 * Set incremental counter value
 */
IO_ErrorType IO_PWD_IncSet(ubyte1 inc_channel, ubyte2 count)
{
    int idx = complex_pwd_to_index(inc_channel);
    
    if (idx < 0)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    if (!complex_pwd_states[idx].initialized || complex_pwd_states[idx].mode != PWD_MODE_INCREMENTAL)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    complex_pwd_states[idx].counter_value = count;
    
    return IO_E_OK;
}

/**
 * Deinitialize incremental encoder
 */
IO_ErrorType IO_PWD_IncDeInit(ubyte1 inc_channel)
{
    int idx = complex_pwd_to_index(inc_channel);
    
    if (idx < 0)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    if (!complex_pwd_states[idx].initialized || complex_pwd_states[idx].mode != PWD_MODE_INCREMENTAL)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Clear neighbor channel
    ubyte1 neighbor = complex_pwd_states[idx].neighbor_channel;
    int neighbor_idx = complex_pwd_to_index(neighbor);
    if (neighbor_idx >= 0)
    {
        memset(&complex_pwd_states[neighbor_idx], 0, sizeof(Complex_PWD_State));
    }
    
    // Clear primary channel
    memset(&complex_pwd_states[idx], 0, sizeof(Complex_PWD_State));
    
    return IO_E_OK;
}

// ---------------- Counter Functions (IO_PWD_08 and IO_PWD_10 only) ----------------

/**
 * Setup counter
 */
IO_ErrorType IO_PWD_CountInit(ubyte1 count_channel, ubyte1 mode, ubyte1 direction,
                             ubyte2 count_init, ubyte1 threshold, ubyte1 pupd,
                             IO_PWD_INC_SAFETY_CONF const * const safety_conf)
{
    // Only IO_PWD_08 and IO_PWD_10 support counter mode
    if (count_channel != IO_PWD_08 && count_channel != IO_PWD_10)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    int idx = complex_pwd_to_index(count_channel);
    
    if (complex_pwd_states[idx].initialized)
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    // Validate parameters
    if (mode != IO_PWD_RISING_COUNT && mode != IO_PWD_FALLING_COUNT && mode != IO_PWD_BOTH_COUNT)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    if (direction != IO_PWD_UP_COUNT && direction != IO_PWD_DOWN_COUNT)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    complex_pwd_states[idx].initialized = TRUE;
    complex_pwd_states[idx].mode = PWD_MODE_COUNTER;
    complex_pwd_states[idx].inc_mode = mode;
    complex_pwd_states[idx].count_direction = direction;
    complex_pwd_states[idx].counter_value = count_init;
    complex_pwd_states[idx].threshold = threshold;
    complex_pwd_states[idx].pupd = pupd;
    
    return IO_E_OK;
}

/**
 * Get counter value
 */
IO_ErrorType IO_PWD_CountGet(ubyte1 count_channel, ubyte2 * const count)
{
    if (count == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    if (count_channel != IO_PWD_08 && count_channel != IO_PWD_10)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    int idx = complex_pwd_to_index(count_channel);
    
    if (!complex_pwd_states[idx].initialized || complex_pwd_states[idx].mode != PWD_MODE_COUNTER)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    *count = complex_pwd_states[idx].counter_value;
    
    return IO_E_OK;
}

/**
 * Set counter value
 */
IO_ErrorType IO_PWD_CountSet(ubyte1 count_channel, ubyte2 count)
{
    if (count_channel != IO_PWD_08 && count_channel != IO_PWD_10)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    int idx = complex_pwd_to_index(count_channel);
    
    if (!complex_pwd_states[idx].initialized || complex_pwd_states[idx].mode != PWD_MODE_COUNTER)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    complex_pwd_states[idx].counter_value = count;
    
    return IO_E_OK;
}

/**
 * Deinitialize counter
 */
IO_ErrorType IO_PWD_CountDeInit(ubyte1 count_channel)
{
    if (count_channel != IO_PWD_08 && count_channel != IO_PWD_10)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    int idx = complex_pwd_to_index(count_channel);
    
    if (!complex_pwd_states[idx].initialized || complex_pwd_states[idx].mode != PWD_MODE_COUNTER)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    memset(&complex_pwd_states[idx], 0, sizeof(Complex_PWD_State));
    
    return IO_E_OK;
}

// ---------------- Helper Functions for Testing ----------------

/**
 * SIL-specific function to set simulated frequency for testing
 */
void IO_PWD_SIL_SetFreq(ubyte1 channel, ubyte2 frequency_hz)
{
    int idx = simple_pwd_to_index(channel);
    if (idx >= 0)
    {
        simple_pwd_states[idx].simulated_freq = frequency_hz;
        return;
    }
    
    idx = complex_pwd_to_index(channel);
    if (idx >= 0)
    {
        complex_pwd_states[idx].simulated_freq = frequency_hz;
    }
}

/**
 * SIL-specific function to set simulated pulse width for testing
 */
void IO_PWD_SIL_SetPulse(ubyte1 channel, ubyte4 pulse_us)
{
    int idx = simple_pwd_to_index(channel);
    if (idx >= 0)
    {
        simple_pwd_states[idx].simulated_pulse = pulse_us;
        return;
    }
    
    idx = complex_pwd_to_index(channel);
    if (idx >= 0)
    {
        complex_pwd_states[idx].simulated_pulse = pulse_us;
    }
}

/**
 * SIL-specific function to simulate encoder/counter increment for testing
 */
void IO_PWD_SIL_Increment(ubyte1 channel, sbyte2 delta)
{
    int idx = complex_pwd_to_index(channel);
    if (idx >= 0 && (complex_pwd_states[idx].mode == PWD_MODE_INCREMENTAL ||
                     complex_pwd_states[idx].mode == PWD_MODE_COUNTER))
    {
        complex_pwd_states[idx].counter_value = (ubyte2)((sbyte4)complex_pwd_states[idx].counter_value + delta);
    }
}

