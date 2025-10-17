#include <string.h>
#include "IO_PWM.h"
#include "IO_Constants.h"

typedef unsigned char ubyte1;
typedef unsigned int ubyte2;
typedef unsigned long ubyte4;

// ---------------- Configuration ----------------

#define MAX_PWM_CHANNELS 8
#define MAX_CURRENT_CHANNELS 4

// PWM mode types
typedef enum {
    PWM_MODE_NONE = 0,
    PWM_MODE_SINGLE,
    PWM_MODE_DUAL_PRIMARY,
    PWM_MODE_DUAL_SECONDARY
} PWM_Mode;

// PWM channel state
typedef struct {
    bool initialized;
    PWM_Mode mode;
    ubyte2 frequency;           // PWM frequency in Hz
    bool polarity;
    bool cur_measurement;
    ubyte1 cur_channel;
    bool diag_margin;
    ubyte2 duty_cycle;          // Current duty cycle (0..65535)
    ubyte1 neighbor_channel;    // For dual mode
} PWM_Channel_State;

// Current measurement state
typedef struct {
    bool initialized;
    ubyte1 pwm_channel;
    ubyte2 simulated_current;   // Simulated current in mA (0..2500)
} Current_Channel_State;

// Global state
static PWM_Channel_State pwm_states[MAX_PWM_CHANNELS];
static Current_Channel_State current_states[MAX_CURRENT_CHANNELS];

// ---------------- Helper Functions ----------------

// Convert PWM channel to index
static int pwm_channel_to_index(ubyte1 pwm_channel)
{
    if (pwm_channel >= IO_PWM_00 && pwm_channel <= IO_PWM_07)
    {
        return pwm_channel - IO_PWM_00;
    }
    return -1;
}

// Convert current channel to index
static int cur_channel_to_index(ubyte1 cur_channel)
{
    if (cur_channel >= IO_ADC_CUR_00 && cur_channel <= IO_ADC_CUR_03)
    {
        return cur_channel - IO_ADC_CUR_00;
    }
    return -1;
}

// Validate PWM channel
static bool is_valid_pwm_channel(ubyte1 pwm_channel)
{
    return (pwm_channel >= IO_PWM_00 && pwm_channel <= IO_PWM_07);
}

// Validate current channel
static bool is_valid_cur_channel(ubyte1 cur_channel)
{
    return (cur_channel >= IO_ADC_CUR_00 && cur_channel <= IO_ADC_CUR_03);
}

// Validate frequency (50Hz to 1000Hz)
static bool is_valid_frequency(ubyte2 frequency)
{
    return (frequency >= 50 && frequency <= 1000);
}

// Check if PWM channel is initialized
static bool is_pwm_initialized(ubyte1 pwm_channel)
{
    int idx = pwm_channel_to_index(pwm_channel);
    if (idx < 0 || idx >= MAX_PWM_CHANNELS) return FALSE;
    return pwm_states[idx].initialized;
}

// Check if current channel is initialized
static bool is_cur_initialized(ubyte1 cur_channel)
{
    int idx = cur_channel_to_index(cur_channel);
    if (idx < 0 || idx >= MAX_CURRENT_CHANNELS) return FALSE;
    return current_states[idx].initialized;
}

// Get neighbor channel for dual mode
static ubyte1 get_neighbor_channel(ubyte1 pwm_channel)
{
    int idx = pwm_channel_to_index(pwm_channel);
    if (idx < 0) return 0xFF;
    
    // Even channels pair with next odd, odd channels pair with previous even
    if (idx % 2 == 0)
    {
        return pwm_channel + 1;
    }
    else
    {
        return pwm_channel - 1;
    }
}

// Check if channels share frequency time base
static bool channels_share_frequency(ubyte1 ch1, ubyte1 ch2)
{
    int idx1 = pwm_channel_to_index(ch1);
    int idx2 = pwm_channel_to_index(ch2);
    
    if (idx1 < 0 || idx2 < 0) return FALSE;
    
    // IO_PWM_00..03 have independent time bases
    if (idx1 <= 3 && idx2 <= 3) return FALSE;
    
    // IO_PWM_04 and IO_PWM_05 share time base
    if ((idx1 == 4 || idx1 == 5) && (idx2 == 4 || idx2 == 5)) return TRUE;
    
    // IO_PWM_06 and IO_PWM_07 share time base
    if ((idx1 == 6 || idx1 == 7) && (idx2 == 6 || idx2 == 7)) return TRUE;
    
    return FALSE;
}

// ---------------- Main Functions ----------------

/**
 * Setup single PWM output
 */
IO_ErrorType IO_PWM_Init(ubyte1 pwm_channel, ubyte2 frequency, bool polarity,
                        bool cur_measurement, ubyte1 cur_channel, bool diag_margin,
                        IO_PWM_SAFETY_CONF const * const safety_conf)
{
    // Validate PWM channel
    if (!is_valid_pwm_channel(pwm_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if already initialized
    if (is_pwm_initialized(pwm_channel))
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    // Validate frequency
    if (!is_valid_frequency(frequency))
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    // Validate current measurement configuration
    if (cur_measurement)
    {
        if (!is_valid_cur_channel(cur_channel))
        {
            return IO_E_INVALID_CHANNEL_ID;
        }
        
        if (is_cur_initialized(cur_channel))
        {
            return IO_E_CHANNEL_BUSY;
        }
    }
    
    // If safety_conf is provided, force diag_margin to TRUE
    if (safety_conf != NULL)
    {
        diag_margin = TRUE;
    }
    
    // Initialize PWM channel
    int idx = pwm_channel_to_index(pwm_channel);
    if (idx >= 0 && idx < MAX_PWM_CHANNELS)
    {
        pwm_states[idx].initialized = TRUE;
        pwm_states[idx].mode = PWM_MODE_SINGLE;
        pwm_states[idx].frequency = frequency;
        pwm_states[idx].polarity = polarity;
        pwm_states[idx].cur_measurement = cur_measurement;
        pwm_states[idx].cur_channel = cur_channel;
        pwm_states[idx].diag_margin = diag_margin;
        pwm_states[idx].duty_cycle = 0;
        pwm_states[idx].neighbor_channel = 0xFF;
        
        // Initialize current measurement if enabled
        if (cur_measurement)
        {
            int cur_idx = cur_channel_to_index(cur_channel);
            if (cur_idx >= 0 && cur_idx < MAX_CURRENT_CHANNELS)
            {
                current_states[cur_idx].initialized = TRUE;
                current_states[cur_idx].pwm_channel = pwm_channel;
                current_states[cur_idx].simulated_current = 0;
            }
        }
    }
    
    return IO_E_OK;
}

/**
 * Setup two PWM outputs for alternating driving
 */
IO_ErrorType IO_PWM_DualInit(ubyte1 pwm_channel, ubyte2 frequency, bool polarity,
                            ubyte1 cur_channel, IO_PWM_SAFETY_CONF const * const safety_conf)
{
    // Validate PWM channel
    if (!is_valid_pwm_channel(pwm_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Get neighbor channel
    ubyte1 neighbor = get_neighbor_channel(pwm_channel);
    if (!is_valid_pwm_channel(neighbor))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if already initialized
    if (is_pwm_initialized(pwm_channel) || is_pwm_initialized(neighbor))
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    // Validate frequency
    if (!is_valid_frequency(frequency))
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    // Validate current channel
    if (!is_valid_cur_channel(cur_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    if (is_cur_initialized(cur_channel))
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    // Initialize primary channel
    int idx = pwm_channel_to_index(pwm_channel);
    int neighbor_idx = pwm_channel_to_index(neighbor);
    
    if (idx >= 0 && idx < MAX_PWM_CHANNELS)
    {
        pwm_states[idx].initialized = TRUE;
        pwm_states[idx].mode = PWM_MODE_DUAL_PRIMARY;
        pwm_states[idx].frequency = frequency;
        pwm_states[idx].polarity = polarity;
        pwm_states[idx].cur_measurement = TRUE;
        pwm_states[idx].cur_channel = cur_channel;
        pwm_states[idx].diag_margin = TRUE; // Always TRUE for dual mode
        pwm_states[idx].duty_cycle = 0;
        pwm_states[idx].neighbor_channel = neighbor;
    }
    
    // Initialize secondary channel
    if (neighbor_idx >= 0 && neighbor_idx < MAX_PWM_CHANNELS)
    {
        pwm_states[neighbor_idx].initialized = TRUE;
        pwm_states[neighbor_idx].mode = PWM_MODE_DUAL_SECONDARY;
        pwm_states[neighbor_idx].frequency = frequency;
        pwm_states[neighbor_idx].polarity = polarity;
        pwm_states[neighbor_idx].cur_measurement = TRUE;
        pwm_states[neighbor_idx].cur_channel = cur_channel;
        pwm_states[neighbor_idx].diag_margin = TRUE;
        pwm_states[neighbor_idx].duty_cycle = 0;
        pwm_states[neighbor_idx].neighbor_channel = pwm_channel;
    }
    
    // Initialize current measurement
    int cur_idx = cur_channel_to_index(cur_channel);
    if (cur_idx >= 0 && cur_idx < MAX_CURRENT_CHANNELS)
    {
        current_states[cur_idx].initialized = TRUE;
        current_states[cur_idx].pwm_channel = pwm_channel;
        current_states[cur_idx].simulated_current = 0;
    }
    
    return IO_E_OK;
}

/**
 * Deinitializes a PWM output
 */
IO_ErrorType IO_PWM_DeInit(ubyte1 pwm_channel)
{
    // Validate channel
    if (!is_valid_pwm_channel(pwm_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if initialized
    if (!is_pwm_initialized(pwm_channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    int idx = pwm_channel_to_index(pwm_channel);
    if (idx < 0 || idx >= MAX_PWM_CHANNELS)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Deinitialize current measurement if configured
    if (pwm_states[idx].cur_measurement)
    {
        int cur_idx = cur_channel_to_index(pwm_states[idx].cur_channel);
        if (cur_idx >= 0 && cur_idx < MAX_CURRENT_CHANNELS)
        {
            memset(&current_states[cur_idx], 0, sizeof(Current_Channel_State));
        }
    }
    
    // Clear PWM state
    memset(&pwm_states[idx], 0, sizeof(PWM_Channel_State));
    
    return IO_E_OK;
}

/**
 * Deinitializes two PWM outputs for alternating driving
 */
IO_ErrorType IO_PWM_DualDeInit(ubyte1 pwm_channel)
{
    // Validate channel
    if (!is_valid_pwm_channel(pwm_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if initialized
    if (!is_pwm_initialized(pwm_channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    int idx = pwm_channel_to_index(pwm_channel);
    if (idx < 0 || idx >= MAX_PWM_CHANNELS)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Get neighbor channel
    ubyte1 neighbor = pwm_states[idx].neighbor_channel;
    
    // Deinitialize current measurement
    int cur_idx = cur_channel_to_index(pwm_states[idx].cur_channel);
    if (cur_idx >= 0 && cur_idx < MAX_CURRENT_CHANNELS)
    {
        memset(&current_states[cur_idx], 0, sizeof(Current_Channel_State));
    }
    
    // Clear primary channel
    memset(&pwm_states[idx], 0, sizeof(PWM_Channel_State));
    
    // Clear secondary channel
    if (is_valid_pwm_channel(neighbor))
    {
        int neighbor_idx = pwm_channel_to_index(neighbor);
        if (neighbor_idx >= 0 && neighbor_idx < MAX_PWM_CHANNELS)
        {
            memset(&pwm_states[neighbor_idx], 0, sizeof(PWM_Channel_State));
        }
    }
    
    return IO_E_OK;
}

/**
 * Set the duty cycle for a PWM channel
 */
IO_ErrorType IO_PWM_SetDuty(ubyte1 pwm_channel, ubyte2 duty_cycle, 
                           ubyte4 * const duty_cycle_fb)
{
    // Validate channel
    if (!is_valid_pwm_channel(pwm_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if initialized
    if (!is_pwm_initialized(pwm_channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    int idx = pwm_channel_to_index(pwm_channel);
    if (idx < 0 || idx >= MAX_PWM_CHANNELS)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Set duty cycle
    pwm_states[idx].duty_cycle = duty_cycle;
    
    // Calculate duty cycle feedback (in microseconds)
    if (duty_cycle_fb != NULL && pwm_states[idx].frequency > 0)
    {
        // Calculate period in microseconds
        ubyte4 period_us = 1000000UL / pwm_states[idx].frequency;
        
        // Calculate duty cycle in microseconds
        *duty_cycle_fb = (ubyte4)((period_us * (ubyte4)duty_cycle) / 65535UL);
    }
    
    return IO_E_OK;
}

/**
 * Set the duty cycle for two PWM outputs for alternating driving
 */
IO_ErrorType IO_PWM_DualSetDuty(ubyte1 pwm_channel, ubyte2 duty_cycle, bool direction,
                               ubyte4 * const duty_cycle_fb_0, ubyte4 * const duty_cycle_fb_1)
{
    // Validate channel
    if (!is_valid_pwm_channel(pwm_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if initialized
    if (!is_pwm_initialized(pwm_channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    int idx = pwm_channel_to_index(pwm_channel);
    if (idx < 0 || idx >= MAX_PWM_CHANNELS)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if in dual mode
    if (pwm_states[idx].mode != PWM_MODE_DUAL_PRIMARY)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    ubyte1 neighbor = pwm_states[idx].neighbor_channel;
    int neighbor_idx = pwm_channel_to_index(neighbor);
    
    if (neighbor_idx < 0 || neighbor_idx >= MAX_PWM_CHANNELS)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Set duty cycle based on direction
    if (direction)
    {
        // Active on primary channel
        pwm_states[idx].duty_cycle = duty_cycle;
        pwm_states[neighbor_idx].duty_cycle = 0; // Diagnostic signal only
    }
    else
    {
        // Active on secondary channel
        pwm_states[idx].duty_cycle = 0; // Diagnostic signal only
        pwm_states[neighbor_idx].duty_cycle = duty_cycle;
    }
    
    // Calculate feedback values
    if (pwm_states[idx].frequency > 0)
    {
        ubyte4 period_us = 1000000UL / pwm_states[idx].frequency;
        
        if (duty_cycle_fb_0 != NULL)
        {
            *duty_cycle_fb_0 = (ubyte4)((period_us * (ubyte4)pwm_states[idx].duty_cycle) / 65535UL);
        }
        
        if (duty_cycle_fb_1 != NULL)
        {
            *duty_cycle_fb_1 = (ubyte4)((period_us * (ubyte4)pwm_states[neighbor_idx].duty_cycle) / 65535UL);
        }
    }
    
    return IO_E_OK;
}

/**
 * Returns the measured current of the given channel
 */
IO_ErrorType IO_PWM_GetCur(ubyte1 pwm_channel, ubyte2 * const current)
{
    // Validate pointer
    if (current == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Validate channel
    if (!is_valid_pwm_channel(pwm_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if initialized
    if (!is_pwm_initialized(pwm_channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    int idx = pwm_channel_to_index(pwm_channel);
    if (idx < 0 || idx >= MAX_PWM_CHANNELS)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if current measurement is enabled
    if (!pwm_states[idx].cur_measurement)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Get simulated current value
    int cur_idx = cur_channel_to_index(pwm_states[idx].cur_channel);
    if (cur_idx >= 0 && cur_idx < MAX_CURRENT_CHANNELS)
    {
        *current = current_states[cur_idx].simulated_current;
    }
    else
    {
        *current = 0;
    }
    
    return IO_E_OK;
}

/**
 * Set the frequency for a PWM channel
 */
IO_ErrorType IO_PWM_SetFreq(ubyte1 pwm_channel, ubyte2 frequency)
{
    // Validate channel (only IO_PWM_00..03 supported)
    if (pwm_channel < IO_PWM_00 || pwm_channel > IO_PWM_03)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if initialized
    if (!is_pwm_initialized(pwm_channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Validate frequency (in 1/10 Hz, so 0..10000 means 0..1000 Hz)
    if (frequency > 10000)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    int idx = pwm_channel_to_index(pwm_channel);
    if (idx < 0 || idx >= MAX_PWM_CHANNELS)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Set frequency (convert from 1/10 Hz to Hz)
    pwm_states[idx].frequency = frequency / 10;
    
    return IO_E_OK;
}

// ---------------- Helper Functions for Testing ----------------

/**
 * SIL-specific function to set simulated current for testing
 * This function is not part of the real IO_PWM API
 */
void IO_PWM_SIL_SetCurrent(ubyte1 cur_channel, ubyte2 current_ma)
{
    int idx = cur_channel_to_index(cur_channel);
    if (idx >= 0 && idx < MAX_CURRENT_CHANNELS)
    {
        // Clamp to valid range (0..2500 mA)
        if (current_ma > 2500)
        {
            current_ma = 2500;
        }
        current_states[idx].simulated_current = current_ma;
    }
}

/**
 * SIL-specific function to get current duty cycle for testing
 * This function is not part of the real IO_PWM API
 */
ubyte2 IO_PWM_SIL_GetDuty(ubyte1 pwm_channel)
{
    int idx = pwm_channel_to_index(pwm_channel);
    if (idx >= 0 && idx < MAX_PWM_CHANNELS && pwm_states[idx].initialized)
    {
        return pwm_states[idx].duty_cycle;
    }
    return 0;
}

/**
 * SIL-specific function to get current frequency for testing
 * This function is not part of the real IO_PWM API
 */
ubyte2 IO_PWM_SIL_GetFreq(ubyte1 pwm_channel)
{
    int idx = pwm_channel_to_index(pwm_channel);
    if (idx >= 0 && idx < MAX_PWM_CHANNELS && pwm_states[idx].initialized)
    {
        return pwm_states[idx].frequency;
    }
    return 0;
}

