#include <string.h>
#include "IO_ADC.h"
#include "IO_Constants.h"

typedef unsigned char ubyte1;
typedef unsigned int ubyte2;
typedef unsigned long ubyte4;
typedef signed char sbyte1;
typedef float float4;

// ---------------- Configuration ----------------

// Maximum number of ADC channels
#define MAX_ADC_CHANNELS 32

// Channel tracking structure
typedef struct {
    bool initialized;
    ubyte1 type;
    ubyte1 range;
    ubyte1 pupd;
    ubyte1 sensor_supply;
} ADC_Channel_State;

static ADC_Channel_State channel_states[MAX_ADC_CHANNELS];

// Simulated ADC values for testing (can be modified for different test scenarios)
static ubyte2 simulated_values[MAX_ADC_CHANNELS];

// ---------------- Helper Functions ----------------

// Convert channel ID to internal index
static int channel_to_index(ubyte1 adc_channel)
{
    // IO_ADC_5V_00 .. IO_ADC_5V_07
    if (adc_channel >= IO_ADC_5V_00 && adc_channel <= IO_ADC_5V_07)
    {
        return adc_channel - IO_ADC_5V_00;
    }
    
    // IO_ADC_VAR_00 .. IO_ADC_VAR_07
    if (adc_channel >= IO_ADC_VAR_00 && adc_channel <= IO_ADC_VAR_07)
    {
        return 8 + (adc_channel - IO_ADC_VAR_00);
    }
    
    // IO_ADC_00 .. IO_ADC_11
    if (adc_channel >= IO_ADC_00 && adc_channel <= IO_ADC_11)
    {
        return 16 + (adc_channel - IO_ADC_00);
    }
    
    // Special channels
    if (adc_channel == IO_BOARD_TEMP) return 28;
    if (adc_channel == IO_SENSOR_SUPPLY_VAR) return 29;
    if (adc_channel == IO_ADC_SENSOR_SUPPLY_0) return 30;
    if (adc_channel == IO_ADC_SENSOR_SUPPLY_1) return 31;
    
    // IO_ADC_UBAT and IO_ADC_REF_2V5 would need more slots, 
    // but for simplicity we'll handle them specially
    
    return -1; // Invalid channel
}

// Validate ADC channel
static bool is_valid_channel(ubyte1 adc_channel)
{
    if (adc_channel >= IO_ADC_5V_00 && adc_channel <= IO_ADC_5V_07) return TRUE;
    if (adc_channel >= IO_ADC_VAR_00 && adc_channel <= IO_ADC_VAR_07) return TRUE;
    if (adc_channel >= IO_ADC_00 && adc_channel <= IO_ADC_11) return TRUE;
    if (adc_channel == IO_BOARD_TEMP) return TRUE;
    if (adc_channel == IO_SENSOR_SUPPLY_VAR) return TRUE;
    if (adc_channel == IO_ADC_SENSOR_SUPPLY_0) return TRUE;
    if (adc_channel == IO_ADC_SENSOR_SUPPLY_1) return TRUE;
    if (adc_channel == IO_ADC_UBAT) return TRUE;
    if (adc_channel == IO_ADC_REF_2V5) return TRUE;
    
    return FALSE;
}

// Validate type parameter
static bool is_valid_type(ubyte1 type)
{
    return (type == IO_ADC_RATIOMETRIC || 
            type == IO_ADC_CURRENT || 
            type == IO_ADC_RESISTIVE || 
            type == IO_ADC_ABSOLUTE);
}

// Validate range parameter
static bool is_valid_range(ubyte1 range)
{
    return (range <= IO_ADC_RANGE_MAX);
}

// Validate pull-up/pull-down parameter
static bool is_valid_pupd(ubyte1 pupd)
{
    return (pupd == IO_ADC_PU_10K || 
            pupd == IO_ADC_PD_10K || 
            pupd == IO_ADC_PD_1K8 || 
            pupd == IO_ADC_PD_110);
}

// Validate sensor supply parameter
static bool is_valid_sensor_supply(ubyte1 sensor_supply)
{
    return (sensor_supply == IO_ADC_SENSOR_SUPPLY_0 || 
            sensor_supply == IO_ADC_SENSOR_SUPPLY_1 || 
            sensor_supply == IO_SENSOR_SUPPLY_VAR);
}

// Check if channel is initialized
static bool is_channel_initialized(ubyte1 adc_channel)
{
    int idx = channel_to_index(adc_channel);
    if (idx < 0 || idx >= MAX_ADC_CHANNELS) return FALSE;
    return channel_states[idx].initialized;
}

// Validate parameters based on channel type
static IO_ErrorType validate_channel_config(ubyte1 adc_channel, ubyte1 type, ubyte1 range, 
                                            ubyte1 pupd, ubyte1 sensor_supply)
{
    // IO_ADC_5V_00 .. IO_ADC_5V_07
    if (adc_channel >= IO_ADC_5V_00 && adc_channel <= IO_ADC_5V_07)
    {
        if (!is_valid_type(type)) return IO_E_INVALID_PARAMETER;
        
        // For ratiometric, validate sensor supply
        if (type == IO_ADC_RATIOMETRIC)
        {
            if (sensor_supply != IO_ADC_SENSOR_SUPPLY_0 && 
                sensor_supply != IO_ADC_SENSOR_SUPPLY_1)
            {
                return IO_E_INVALID_PARAMETER;
            }
        }
        return IO_E_OK;
    }
    
    // IO_ADC_VAR_00 .. IO_ADC_VAR_07
    if (adc_channel >= IO_ADC_VAR_00 && adc_channel <= IO_ADC_VAR_07)
    {
        if (type != IO_ADC_RATIOMETRIC && type != IO_ADC_ABSOLUTE)
        {
            return IO_E_INVALID_PARAMETER;
        }
        
        if (!is_valid_range(range)) return IO_E_INVALID_PARAMETER;
        
        if (pupd != IO_ADC_PU_10K && pupd != IO_ADC_PD_10K)
        {
            return IO_E_INVALID_PARAMETER;
        }
        
        // For ratiometric, validate sensor supply
        if (type == IO_ADC_RATIOMETRIC)
        {
            if (!is_valid_sensor_supply(sensor_supply))
            {
                return IO_E_INVALID_PARAMETER;
            }
        }
        return IO_E_OK;
    }
    
    // IO_ADC_00 .. IO_ADC_07
    if (adc_channel >= IO_ADC_00 && adc_channel <= IO_ADC_07)
    {
        if (type != IO_ADC_RATIOMETRIC && type != IO_ADC_ABSOLUTE)
        {
            return IO_E_INVALID_PARAMETER;
        }
        
        // For ratiometric, validate sensor supply
        if (type == IO_ADC_RATIOMETRIC)
        {
            if (sensor_supply != IO_ADC_SENSOR_SUPPLY_0 && 
                sensor_supply != IO_ADC_SENSOR_SUPPLY_1)
            {
                return IO_E_INVALID_PARAMETER;
            }
        }
        return IO_E_OK;
    }
    
    // IO_ADC_08 .. IO_ADC_11 (with pull-up/down support)
    if (adc_channel >= IO_ADC_08 && adc_channel <= IO_ADC_11)
    {
        if (!is_valid_pupd(pupd)) return IO_E_INVALID_PARAMETER;
        return IO_E_OK;
    }
    
    // Special channels - parameters are ignored
    if (adc_channel == IO_BOARD_TEMP) return IO_E_OK;
    if (adc_channel == IO_SENSOR_SUPPLY_VAR) return IO_E_OK;
    if (adc_channel == IO_ADC_SENSOR_SUPPLY_0) return IO_E_OK;
    if (adc_channel == IO_ADC_SENSOR_SUPPLY_1) return IO_E_OK;
    if (adc_channel == IO_ADC_UBAT) return IO_E_OK;
    if (adc_channel == IO_ADC_REF_2V5) return IO_E_OK;
    
    return IO_E_INVALID_CHANNEL_ID;
}

// ---------------- Main Functions ----------------

/**
 * Setup one ADC channel
 */
IO_ErrorType IO_ADC_ChannelInit(ubyte1 adc_channel, 
                                ubyte1 type, 
                                ubyte1 range, 
                                ubyte1 pupd, 
                                ubyte1 sensor_supply, 
                                IO_ADC_SAFETY_CONF const * const safety_conf)
{
    // Validate channel
    if (!is_valid_channel(adc_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Special handling for IO_ADC_UBAT (initialized in IO_Driver_Init)
    if (adc_channel == IO_ADC_UBAT)
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    // Check if already initialized
    if (is_channel_initialized(adc_channel))
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    // Validate configuration based on channel type
    IO_ErrorType result = validate_channel_config(adc_channel, type, range, pupd, sensor_supply);
    if (result != IO_E_OK)
    {
        return result;
    }
    
    // Store channel configuration
    int idx = channel_to_index(adc_channel);
    if (idx >= 0 && idx < MAX_ADC_CHANNELS)
    {
        channel_states[idx].initialized = TRUE;
        channel_states[idx].type = type;
        channel_states[idx].range = range;
        channel_states[idx].pupd = pupd;
        channel_states[idx].sensor_supply = sensor_supply;
        
        // Initialize with default simulated value based on type
        if (type == IO_ADC_ABSOLUTE || type == IO_ADC_RATIOMETRIC)
        {
            simulated_values[idx] = 2500; // 2.5V default
        }
        else if (type == IO_ADC_CURRENT)
        {
            simulated_values[idx] = 10000; // 10mA default
        }
        else if (type == IO_ADC_RESISTIVE)
        {
            simulated_values[idx] = 5000; // 5kOhm default
        }
    }
    
    return IO_E_OK;
}

/**
 * Deinitializes one ADC input
 */
IO_ErrorType IO_ADC_ChannelDeInit(ubyte1 adc_channel)
{
    // Validate channel
    if (!is_valid_channel(adc_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if initialized
    if (!is_channel_initialized(adc_channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Clear channel state
    int idx = channel_to_index(adc_channel);
    if (idx >= 0 && idx < MAX_ADC_CHANNELS)
    {
        memset(&channel_states[idx], 0, sizeof(ADC_Channel_State));
        simulated_values[idx] = 0;
    }
    
    return IO_E_OK;
}

/**
 * Returns the value of the given ADC channel
 */
IO_ErrorType IO_ADC_Get(ubyte1 adc_channel, 
                        ubyte2 * const adc_value, 
                        bool * const fresh)
{
    // Validate pointers
    if (adc_value == NULL || fresh == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Validate channel
    if (!is_valid_channel(adc_channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Special channels can be read without initialization
    if (adc_channel == IO_BOARD_TEMP)
    {
        *adc_value = 12000; // ~20°C: (12000 - 10000) / 100 = 20°C
        *fresh = TRUE;
        return IO_E_OK;
    }
    
    if (adc_channel == IO_SENSOR_SUPPLY_VAR)
    {
        *adc_value = 10000; // 10V
        *fresh = TRUE;
        return IO_E_OK;
    }
    
    if (adc_channel == IO_ADC_SENSOR_SUPPLY_0)
    {
        *adc_value = 5000; // 5V
        *fresh = TRUE;
        return IO_E_OK;
    }
    
    if (adc_channel == IO_ADC_SENSOR_SUPPLY_1)
    {
        *adc_value = 5000; // 5V
        *fresh = TRUE;
        return IO_E_OK;
    }
    
    if (adc_channel == IO_ADC_UBAT)
    {
        *adc_value = 12000; // 12V battery
        *fresh = TRUE;
        return IO_E_OK;
    }
    
    if (adc_channel == IO_ADC_REF_2V5)
    {
        *adc_value = 512; // ~2.5V reference (raw value 0-1023)
        *fresh = TRUE;
        return IO_E_OK;
    }
    
    // Check if initialized for normal channels
    if (!is_channel_initialized(adc_channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Return simulated value
    int idx = channel_to_index(adc_channel);
    if (idx >= 0 && idx < MAX_ADC_CHANNELS)
    {
        *adc_value = simulated_values[idx];
        *fresh = TRUE;
    }
    else
    {
        *adc_value = 0;
        *fresh = FALSE;
    }
    
    return IO_E_OK;
}

/**
 * Calculates the board temperature in float
 */
float4 IO_ADC_BoardTempFloat(ubyte2 raw_value)
{
    // Temperature conversion: (raw_value - 10000) / 100
    return ((float4)raw_value - 10000.0f) / 100.0f;
}

/**
 * Calculates the board temperature in sbyte1
 */
sbyte1 IO_ADC_BoardTempSbyte(ubyte2 raw_value)
{
    // Temperature conversion: (raw_value - 10000) / 100
    // Round to nearest integer
    float4 temp_float = ((float4)raw_value - 10000.0f) / 100.0f;
    
    if (temp_float >= 127.0f) return 127;
    if (temp_float <= -128.0f) return -128;
    
    return (sbyte1)(temp_float + (temp_float >= 0 ? 0.5f : -0.5f));
}

// ---------------- Helper Functions for Testing ----------------

/**
 * SIL-specific function to set a simulated ADC value for testing
 * This function is not part of the real IO_ADC API
 */
void IO_ADC_SIL_SetValue(ubyte1 adc_channel, ubyte2 value)
{
    int idx = channel_to_index(adc_channel);
    if (idx >= 0 && idx < MAX_ADC_CHANNELS)
    {
        simulated_values[idx] = value;
    }
}

