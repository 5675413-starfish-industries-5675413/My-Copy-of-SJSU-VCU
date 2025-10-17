#include <string.h>
#include "IO_DIO.h"
#include "IO_Constants.h"

typedef unsigned char ubyte1;

// ---------------- Configuration ----------------

#define MAX_DI_CHANNELS 20
#define MAX_DO_CHANNELS 20

// Channel state tracking
typedef struct {
    bool initialized;
    ubyte1 mode;
} DI_Channel_State;

typedef struct {
    bool initialized;
    bool output_value;
    ubyte1 toggle_count;  // Track for IO_E_DO_DIAG_NOT_FINISHED
} DO_Channel_State;

// Global state
static DI_Channel_State di_states[MAX_DI_CHANNELS];
static DO_Channel_State do_states[MAX_DO_CHANNELS];

// Simulated input values (can be set for testing)
static bool simulated_di_values[MAX_DI_CHANNELS];

// ---------------- Helper Functions ----------------

// Convert DI channel to index
static int di_channel_to_index(ubyte1 di_channel)
{
    // IO_DI_00 .. IO_DI_07
    if (di_channel >= IO_DI_00 && di_channel <= IO_DI_07)
    {
        return di_channel - IO_DI_00;
    }
    
    // IO_DI_K15
    if (di_channel == IO_DI_K15) return 8;
    
    // IO_DI_08 .. IO_DI_15
    if (di_channel >= IO_DI_08 && di_channel <= IO_DI_15)
    {
        return 9 + (di_channel - IO_DI_08);
    }
    
    // IO_DI_16 .. IO_DI_19
    if (di_channel >= IO_DI_16 && di_channel <= IO_DI_19)
    {
        return 17 + (di_channel - IO_DI_16);
    }
    
    return -1; // Invalid
}

// Convert DO channel to index
static int do_channel_to_index(ubyte1 do_channel)
{
    // IO_DO_00 .. IO_DO_07
    if (do_channel >= IO_DO_00 && do_channel <= IO_DO_07)
    {
        return do_channel - IO_DO_00;
    }
    
    // IO_DO_08 .. IO_DO_11
    if (do_channel >= IO_DO_08 && do_channel <= IO_DO_11)
    {
        return 8 + (do_channel - IO_DO_08);
    }
    
    // IO_DO_12 .. IO_DO_19
    if (do_channel >= IO_DO_12 && do_channel <= IO_DO_19)
    {
        return 12 + (do_channel - IO_DO_12);
    }
    
    return -1; // Invalid
}

// Validate DI channel
static bool is_valid_di_channel(ubyte1 di_channel)
{
    if (di_channel >= IO_DI_00 && di_channel <= IO_DI_07) return TRUE;
    if (di_channel == IO_DI_K15) return TRUE;
    if (di_channel >= IO_DI_08 && di_channel <= IO_DI_15) return TRUE;
    if (di_channel >= IO_DI_16 && di_channel <= IO_DI_19) return TRUE;
    return FALSE;
}

// Validate DO channel
static bool is_valid_do_channel(ubyte1 do_channel)
{
    if (do_channel >= IO_DO_00 && do_channel <= IO_DO_07) return TRUE;
    if (do_channel >= IO_DO_08 && do_channel <= IO_DO_11) return TRUE;
    if (do_channel >= IO_DO_12 && do_channel <= IO_DO_19) return TRUE;
    return FALSE;
}

// Validate pull-up/down mode
static bool is_valid_pupd_mode(ubyte1 mode, ubyte1 di_channel)
{
    // For IO_DI_16 .. IO_DI_19, all modes including IO_DI_PD_110 are valid
    if (di_channel >= IO_DI_16 && di_channel <= IO_DI_19)
    {
        return (mode == IO_DI_PU_10K || mode == IO_DI_PD_10K || 
                mode == IO_DI_PD_1K8 || mode == IO_DI_PD_110);
    }
    
    // For other channels, IO_DI_PD_110 is not valid
    return (mode == IO_DI_PU_10K || mode == IO_DI_PD_10K || mode == IO_DI_PD_1K8);
}

// Check if DI channel is initialized
static bool is_di_initialized(ubyte1 di_channel)
{
    int idx = di_channel_to_index(di_channel);
    if (idx < 0 || idx >= MAX_DI_CHANNELS) return FALSE;
    return di_states[idx].initialized;
}

// Check if DO channel is initialized
static bool is_do_initialized(ubyte1 do_channel)
{
    int idx = do_channel_to_index(do_channel);
    if (idx < 0 || idx >= MAX_DO_CHANNELS) return FALSE;
    return do_states[idx].initialized;
}

// ---------------- Main Functions ----------------

/**
 * Setup the Digital Inputs
 */
IO_ErrorType IO_DI_Init(ubyte1 di_channel, ubyte1 mode)
{
    // Validate channel
    if (!is_valid_di_channel(di_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if already initialized
    if (is_di_initialized(di_channel))
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    // For IO_DI_08..IO_DI_15 and IO_DI_K15, mode is ignored
    bool mode_ignored = (di_channel >= IO_DI_08 && di_channel <= IO_DI_15) || 
                        (di_channel == IO_DI_K15);
    
    // Validate mode parameter (unless it's ignored)
    if (!mode_ignored && !is_valid_pupd_mode(mode, di_channel))
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    // Initialize channel
    int idx = di_channel_to_index(di_channel);
    if (idx >= 0 && idx < MAX_DI_CHANNELS)
    {
        di_states[idx].initialized = TRUE;
        di_states[idx].mode = mode_ignored ? IO_DI_PD_10K : mode; // Default for ignored channels
        simulated_di_values[idx] = FALSE; // Default to low
    }
    
    return IO_E_OK;
}

/**
 * Setup the Digital Outputs
 */
IO_ErrorType IO_DO_Init(ubyte1 do_channel)
{
    // Validate channel
    if (!is_valid_do_channel(do_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if already initialized
    if (is_do_initialized(do_channel))
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    // Initialize channel
    int idx = do_channel_to_index(do_channel);
    if (idx >= 0 && idx < MAX_DO_CHANNELS)
    {
        do_states[idx].initialized = TRUE;
        do_states[idx].output_value = FALSE;
        do_states[idx].toggle_count = 0;
    }
    
    return IO_E_OK;
}

/**
 * Deinitializes a DI channel
 */
IO_ErrorType IO_DI_DeInit(ubyte1 di_channel)
{
    // Validate channel
    if (!is_valid_di_channel(di_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if initialized
    if (!is_di_initialized(di_channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Deinitialize channel
    int idx = di_channel_to_index(di_channel);
    if (idx >= 0 && idx < MAX_DI_CHANNELS)
    {
        memset(&di_states[idx], 0, sizeof(DI_Channel_State));
        simulated_di_values[idx] = FALSE;
    }
    
    return IO_E_OK;
}

/**
 * Deinitializes a DO channel
 */
IO_ErrorType IO_DO_DeInit(ubyte1 do_channel)
{
    // Validate channel
    if (!is_valid_do_channel(do_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if initialized
    if (!is_do_initialized(do_channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Deinitialize channel
    int idx = do_channel_to_index(do_channel);
    if (idx >= 0 && idx < MAX_DO_CHANNELS)
    {
        memset(&do_states[idx], 0, sizeof(DO_Channel_State));
    }
    
    return IO_E_OK;
}

/**
 * Gets the value of a Digital Input
 */
IO_ErrorType IO_DI_Get(ubyte1 di_channel, bool * const di_value)
{
    // Validate pointer
    if (di_value == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Validate channel
    if (!is_valid_di_channel(di_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if initialized
    if (!is_di_initialized(di_channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Get simulated value
    int idx = di_channel_to_index(di_channel);
    if (idx >= 0 && idx < MAX_DI_CHANNELS)
    {
        *di_value = simulated_di_values[idx];
    }
    else
    {
        *di_value = FALSE;
    }
    
    return IO_E_OK;
}

/**
 * Sets the value of a Digital Output
 */
IO_ErrorType IO_DO_Set(ubyte1 do_channel, bool do_value)
{
    // Validate channel
    if (!is_valid_do_channel(do_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if initialized
    if (!is_do_initialized(do_channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    int idx = do_channel_to_index(do_channel);
    if (idx < 0 || idx >= MAX_DO_CHANNELS)
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    DO_Channel_State *state = &do_states[idx];
    
    // Track toggling for IO_DO_00..IO_DO_11 (SPI shift register outputs)
    // These channels need to check for IO_E_DO_DIAG_NOT_FINISHED
    if (do_channel >= IO_DO_00 && do_channel <= IO_DO_11)
    {
        // Check if value is being toggled
        if (state->output_value != do_value)
        {
            state->toggle_count++;
            
            // If toggled 3 or more times in a row, return diagnostic error
            if (state->toggle_count >= 3)
            {
                state->toggle_count = 0; // Reset counter
                return IO_E_DO_DIAG_NOT_FINISHED;
            }
        }
        else
        {
            // Same value, reset toggle counter
            state->toggle_count = 0;
        }
    }
    
    // Set the output value
    state->output_value = do_value;
    
    return IO_E_OK;
}

// ---------------- Helper Functions for Testing ----------------

/**
 * SIL-specific function to set a simulated digital input value for testing
 * This function is not part of the real IO_DIO API
 */
void IO_DI_SIL_SetValue(ubyte1 di_channel, bool value)
{
    int idx = di_channel_to_index(di_channel);
    if (idx >= 0 && idx < MAX_DI_CHANNELS)
    {
        simulated_di_values[idx] = value;
    }
}

/**
 * SIL-specific function to get the current digital output value for testing
 * This function is not part of the real IO_DIO API
 */
bool IO_DO_SIL_GetValue(ubyte1 do_channel)
{
    int idx = do_channel_to_index(do_channel);
    if (idx >= 0 && idx < MAX_DO_CHANNELS && do_states[idx].initialized)
    {
        return do_states[idx].output_value;
    }
    return FALSE;
}

/**
 * SIL-specific function to reset toggle counter for testing
 * This function is not part of the real IO_DIO API
 */
void IO_DO_SIL_ResetToggleCount(ubyte1 do_channel)
{
    int idx = do_channel_to_index(do_channel);
    if (idx >= 0 && idx < MAX_DO_CHANNELS)
    {
        do_states[idx].toggle_count = 0;
    }
}

