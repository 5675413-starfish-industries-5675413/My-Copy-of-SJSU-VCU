#include "IO_POWER.h"
#include "IO_Constants.h"

typedef unsigned char ubyte1;

// ---------------- Configuration ----------------

#define MAX_POWER_PINS 8

// Power pin configuration
typedef struct {
    ubyte1 pin;
    bool valid;
    ubyte1 current_mode;
    bool supports_voltage_levels;  // For variable sensor supply
} Power_Pin_State;

// Global state
static bool power_initialized = FALSE;
static bool power_ic_error = FALSE;

// Track power pin states
static Power_Pin_State power_pins[MAX_POWER_PINS] = {
    {IO_INT_POWERSTAGE_ENABLE, TRUE, IO_POWER_OFF, FALSE},
    {IO_INT_PWM_POWER, TRUE, IO_POWER_OFF, FALSE},
    {IO_PIN_135, TRUE, IO_POWER_OFF, FALSE},  // UGEB1 (Sensor supply 1)
    {IO_PIN_136, TRUE, IO_POWER_OFF, FALSE},  // UGEB0 (Sensor supply 0)
    {IO_PIN_147, TRUE, IO_POWER_OFF, FALSE},  // UGEB1 (alternate)
    {IO_PIN_148, TRUE, IO_POWER_OFF, FALSE},  // UGEB0 (alternate)
    {IO_PIN_269, TRUE, IO_POWER_OFF, TRUE},   // UGEB_V (Variable sensor supply)
    {IO_PIN_271, TRUE, IO_POWER_ON, FALSE}    // K15E (Power control, default ON)
};

// ---------------- Helper Functions ----------------

// Find power pin state by pin number
static Power_Pin_State* find_power_pin(ubyte1 pin)
{
    for (ubyte1 i = 0; i < MAX_POWER_PINS; i++)
    {
        if (power_pins[i].pin == pin && power_pins[i].valid)
        {
            return &power_pins[i];
        }
    }
    return NULL;
}

// Validate power mode for a specific pin
static bool is_valid_mode_for_pin(ubyte1 pin, ubyte1 mode)
{
    Power_Pin_State *pin_state = find_power_pin(pin);
    
    if (pin_state == NULL)
    {
        return FALSE;
    }
    
    // Variable sensor supply (IO_PIN_269) only accepts voltage levels
    if (pin_state->supports_voltage_levels)
    {
        return (mode == IO_POWER_8_5_V || 
                mode == IO_POWER_10_0_V || 
                mode == IO_POWER_14_5_V);
    }
    
    // Other pins only accept ON/OFF
    return (mode == IO_POWER_ON || mode == IO_POWER_OFF);
}

// ---------------- Main Functions ----------------

/**
 * Initialization of the power module driver
 */
IO_ErrorType IO_POWER_Init(void)
{
    // Check if already initialized
    if (power_initialized)
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    // Initialize power IC state
    power_initialized = TRUE;
    power_ic_error = FALSE;
    
    // Reset all power pins to default states
    // Sensor supplies and power stages default to OFF
    for (ubyte1 i = 0; i < MAX_POWER_PINS; i++)
    {
        // K15 defaults to ON (powered), others OFF
        if (power_pins[i].pin == IO_PIN_271)
        {
            power_pins[i].current_mode = IO_POWER_ON;
        }
        else if (power_pins[i].supports_voltage_levels)
        {
            power_pins[i].current_mode = IO_POWER_10_0_V;  // Default 10V for variable supply
        }
        else
        {
            power_pins[i].current_mode = IO_POWER_OFF;
        }
    }
    
    return IO_E_OK;
}

/**
 * Deinitializes the power module driver
 */
IO_ErrorType IO_POWER_DeInit(void)
{
    // Check if initialized
    if (!power_initialized)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Reset state
    power_initialized = FALSE;
    power_ic_error = FALSE;
    
    return IO_E_OK;
}

/**
 * Sets a certain mode of the Power IC
 */
IO_ErrorType IO_POWER_Set(ubyte1 pin, ubyte1 mode)
{
    // Check if initialized
    if (!power_initialized)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Find the power pin
    Power_Pin_State *pin_state = find_power_pin(pin);
    
    if (pin_state == NULL)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    // Validate mode for this pin
    if (!is_valid_mode_for_pin(pin, mode))
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    // Set the mode
    pin_state->current_mode = mode;
    
    // Special handling for K15 (power down)
    if (pin == IO_PIN_271 && mode == IO_POWER_OFF)
    {
        // In real hardware, this would shut down the ECU
        // In SIL, we just track the state
        // The application should check IO_DI_K15 before calling this
    }
    
    return IO_E_OK;
}

/**
 * Returns the status of the Power IC
 */
IO_ErrorType IO_POWER_GetStatus(void)
{
    // Check if initialized
    if (!power_initialized)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Check for simulated error
    if (power_ic_error)
    {
        return IO_E_POWER_IC;
    }
    
    return IO_E_OK;
}

// ---------------- Helper Functions for Testing ----------------

/**
 * SIL-specific function to get current power mode for a pin
 * This function is not part of the real IO_POWER API
 */
ubyte1 IO_POWER_SIL_GetMode(ubyte1 pin)
{
    Power_Pin_State *pin_state = find_power_pin(pin);
    
    if (pin_state != NULL)
    {
        return pin_state->current_mode;
    }
    
    return 0xFF;  // Invalid
}

/**
 * SIL-specific function to check if a pin is powered on
 * This function is not part of the real IO_POWER API
 */
bool IO_POWER_SIL_IsOn(ubyte1 pin)
{
    Power_Pin_State *pin_state = find_power_pin(pin);
    
    if (pin_state != NULL)
    {
        // Variable supply is "on" if it has a voltage level set
        if (pin_state->supports_voltage_levels)
        {
            return (pin_state->current_mode == IO_POWER_8_5_V ||
                    pin_state->current_mode == IO_POWER_10_0_V ||
                    pin_state->current_mode == IO_POWER_14_5_V);
        }
        
        // Normal pins are on if mode is IO_POWER_ON
        return (pin_state->current_mode == IO_POWER_ON);
    }
    
    return FALSE;
}

/**
 * SIL-specific function to get variable sensor supply voltage
 * Returns voltage in millivolts, or 0 if not applicable
 * This function is not part of the real IO_POWER API
 */
ubyte2 IO_POWER_SIL_GetVoltage(ubyte1 pin)
{
    Power_Pin_State *pin_state = find_power_pin(pin);
    
    if (pin_state != NULL && pin_state->supports_voltage_levels)
    {
        switch (pin_state->current_mode)
        {
            case IO_POWER_8_5_V:  return 8500;   // 8.5V in mV
            case IO_POWER_10_0_V: return 10000;  // 10.0V in mV
            case IO_POWER_14_5_V: return 14500;  // 14.5V in mV
            default: return 0;
        }
    }
    
    // For non-variable supplies, return standard voltages
    if (pin_state != NULL && IO_POWER_SIL_IsOn(pin))
    {
        // Sensor supplies are typically 5V
        if (pin == IO_PIN_135 || pin == IO_PIN_136 || 
            pin == IO_PIN_147 || pin == IO_PIN_148)
        {
            return 5000;  // 5V
        }
    }
    
    return 0;
}

/**
 * SIL-specific function to simulate a Power IC error
 * This function is not part of the real IO_POWER API
 */
void IO_POWER_SIL_SetError(bool error_state)
{
    power_ic_error = error_state;
}

/**
 * SIL-specific function to check if ECU is powered down
 * This function is not part of the real IO_POWER API
 */
bool IO_POWER_SIL_IsPoweredDown(void)
{
    Power_Pin_State *k15_state = find_power_pin(IO_PIN_271);
    
    if (k15_state != NULL)
    {
        return (k15_state->current_mode == IO_POWER_OFF);
    }
    
    return FALSE;
}

/**
 * SIL-specific function to check if power stages are enabled
 * This function is not part of the real IO_POWER API
 */
bool IO_POWER_SIL_ArePowerStagesEnabled(void)
{
    return IO_POWER_SIL_IsOn(IO_INT_POWERSTAGE_ENABLE);
}

/**
 * SIL-specific function to check if PWM power is enabled
 * This function is not part of the real IO_POWER API
 */
bool IO_POWER_SIL_IsPWMPowerEnabled(void)
{
    return IO_POWER_SIL_IsOn(IO_INT_PWM_POWER);
}

/**
 * SIL-specific function to reset all power pins to default state
 * This function is not part of the real IO_POWER API
 */
void IO_POWER_SIL_ResetAll(void)
{
    for (ubyte1 i = 0; i < MAX_POWER_PINS; i++)
    {
        // K15 defaults to ON (powered), others OFF
        if (power_pins[i].pin == IO_PIN_271)
        {
            power_pins[i].current_mode = IO_POWER_ON;
        }
        else if (power_pins[i].supports_voltage_levels)
        {
            power_pins[i].current_mode = IO_POWER_10_0_V;  // Default 10V
        }
        else
        {
            power_pins[i].current_mode = IO_POWER_OFF;
        }
    }
    
    power_ic_error = FALSE;
}

