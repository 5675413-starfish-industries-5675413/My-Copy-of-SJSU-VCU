#include "DIAG_Functions.h"
#include "DIAG_Constants.h"
#include "IO_Constants.h"

typedef unsigned char ubyte1;

// ---------------- Configuration ----------------

// Diagnostic state tracking
typedef struct {
    ubyte1 diag_state;
    ubyte1 watchdog_state;
    DIAG_ERRORCODE diag_error;
    DIAG_ERRORCODE watchdog_error;
    bool in_safe_state;
    ubyte1 wd_version_major;
    ubyte1 wd_version_minor;
    bool wd_version_available;
} DIAG_State;

// Global state
static DIAG_State diag_state = {
    DIAG_STATE_MAIN,           // Default to main state
    DIAG_WD_STATE_RUNNING,     // Watchdog running
    {DIAG_E_NO_ERROR, 0, 0},   // No diagnostic errors
    {DIAG_WD_E_NO_ERROR, 0, 0}, // No watchdog errors
    FALSE,                     // Not in safe state
    1,                         // Default WD version 1.0
    0,
    TRUE                       // Version available in SIL
};

// ---------------- Main Functions ----------------

/**
 * Status function for diagnostic state machine
 */
IO_ErrorType DIAG_Status(ubyte1 * diag_state_out, ubyte1 * watchdog_state,
                         DIAG_ERRORCODE * diag_error, DIAG_ERRORCODE * watchdog_error)
{
    // Validate pointers
    if (diag_state_out == NULL || watchdog_state == NULL || 
        diag_error == NULL || watchdog_error == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Return current state
    *diag_state_out = diag_state.diag_state;
    *watchdog_state = diag_state.watchdog_state;
    
    // Copy error codes
    diag_error->error_code = diag_state.diag_error.error_code;
    diag_error->device_num = diag_state.diag_error.device_num;
    diag_error->faulty_value = diag_state.diag_error.faulty_value;
    
    watchdog_error->error_code = diag_state.watchdog_error.error_code;
    watchdog_error->device_num = diag_state.watchdog_error.device_num;
    watchdog_error->faulty_value = diag_state.watchdog_error.faulty_value;
    
    return IO_E_OK;
}

/**
 * Allows an application driven safe state
 */
IO_ErrorType DIAG_EnterSafestate(void)
{
    // Enter safe state
    diag_state.diag_state = DIAG_STATE_SAFE_STATE;
    diag_state.in_safe_state = TRUE;
    
    // Set error code to indicate application requested safe state
    diag_state.diag_error.error_code = DIAG_E_APPL_SAFE_STATE;
    diag_state.diag_error.device_num = DIAG_DEV_MAIN_CPU;
    diag_state.diag_error.faulty_value = 0;
    
    return IO_E_OK;
}

/**
 * Returns the version number of the watchdog CPU software
 */
IO_ErrorType DIAG_GetWDVersion(ubyte1 * const wd_ver_maj, ubyte1 * const wd_ver_min)
{
    // Validate pointers
    if (wd_ver_maj == NULL || wd_ver_min == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Check if version is available
    if (!diag_state.wd_version_available)
    {
        return IO_E_WD_NO_VERSION;
    }
    
    // Return version
    *wd_ver_maj = diag_state.wd_version_major;
    *wd_ver_min = diag_state.wd_version_minor;
    
    return IO_E_OK;
}

// ---------------- Helper Functions for Testing ----------------

/**
 * SIL-specific function to set diagnostic state
 * This function is not part of the real DIAG API
 */
void DIAG_SIL_SetState(ubyte1 new_diag_state, ubyte1 new_watchdog_state)
{
    diag_state.diag_state = new_diag_state;
    diag_state.watchdog_state = new_watchdog_state;
}

/**
 * SIL-specific function to set diagnostic error
 * This function is not part of the real DIAG API
 */
void DIAG_SIL_SetDiagError(ubyte1 error_code, ubyte1 device_num, ubyte2 faulty_value)
{
    diag_state.diag_error.error_code = error_code;
    diag_state.diag_error.device_num = device_num;
    diag_state.diag_error.faulty_value = faulty_value;
}

/**
 * SIL-specific function to set watchdog error
 * This function is not part of the real DIAG API
 */
void DIAG_SIL_SetWatchdogError(ubyte1 error_code, ubyte1 device_num, ubyte2 faulty_value)
{
    diag_state.watchdog_error.error_code = error_code;
    diag_state.watchdog_error.device_num = device_num;
    diag_state.watchdog_error.faulty_value = faulty_value;
}

/**
 * SIL-specific function to clear all errors
 * This function is not part of the real DIAG API
 */
void DIAG_SIL_ClearErrors(void)
{
    diag_state.diag_error.error_code = DIAG_E_NO_ERROR;
    diag_state.diag_error.device_num = 0;
    diag_state.diag_error.faulty_value = 0;
    
    diag_state.watchdog_error.error_code = DIAG_WD_E_NO_ERROR;
    diag_state.watchdog_error.device_num = 0;
    diag_state.watchdog_error.faulty_value = 0;
}

/**
 * SIL-specific function to set watchdog version
 * This function is not part of the real DIAG API
 */
void DIAG_SIL_SetWDVersion(ubyte1 major, ubyte1 minor)
{
    diag_state.wd_version_major = major;
    diag_state.wd_version_minor = minor;
    diag_state.wd_version_available = TRUE;
}

/**
 * SIL-specific function to simulate watchdog version not available
 * This function is not part of the real DIAG API
 */
void DIAG_SIL_SetWDVersionUnavailable(void)
{
    diag_state.wd_version_available = FALSE;
}

/**
 * SIL-specific function to check if in safe state
 * This function is not part of the real DIAG API
 */
bool DIAG_SIL_IsInSafeState(void)
{
    return diag_state.in_safe_state;
}

/**
 * SIL-specific function to reset to normal operating state
 * This function is not part of the real DIAG API
 */
void DIAG_SIL_ResetToNormal(void)
{
    diag_state.diag_state = DIAG_STATE_MAIN;
    diag_state.watchdog_state = DIAG_WD_STATE_RUNNING;
    diag_state.in_safe_state = FALSE;
    DIAG_SIL_ClearErrors();
}

/**
 * SIL-specific function to simulate various diagnostic states
 * This function is not part of the real DIAG API
 */
void DIAG_SIL_SimulateInit(void)
{
    diag_state.diag_state = DIAG_STATE_INIT;
    diag_state.watchdog_state = DIAG_WD_STATE_INIT;
}

void DIAG_SIL_SimulateConfig(void)
{
    diag_state.diag_state = DIAG_STATE_CONFIG;
    diag_state.watchdog_state = DIAG_WD_STATE_START_UP;
}

void DIAG_SIL_SimulateMain(void)
{
    diag_state.diag_state = DIAG_STATE_MAIN;
    diag_state.watchdog_state = DIAG_WD_STATE_RUNNING;
}

void DIAG_SIL_SimulateSafeState(void)
{
    diag_state.diag_state = DIAG_STATE_SAFE_STATE;
    diag_state.watchdog_state = DIAG_WD_STATE_SAFE_STATE;
    diag_state.in_safe_state = TRUE;
}

