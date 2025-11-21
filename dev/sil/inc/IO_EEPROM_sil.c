#include <string.h>
#include "IO_EEPROM.h"
#include "IO_Constants.h"

typedef unsigned char ubyte1;
typedef unsigned int ubyte2;

// ---------------- Configuration ----------------

#define EEPROM_SIZE 8192  // 8KB EEPROM

// EEPROM operation state
typedef enum {
    EEPROM_STATE_IDLE = 0,
    EEPROM_STATE_BUSY_READ,
    EEPROM_STATE_BUSY_WRITE
} EEPROM_State;

// Global state
static bool eeprom_initialized = FALSE;
static EEPROM_State eeprom_state = EEPROM_STATE_IDLE;

// Simulated EEPROM memory (persists during program execution)
static ubyte1 eeprom_memory[EEPROM_SIZE];

// Pending operation tracking (for asynchronous simulation)
static ubyte2 pending_offset = 0;
static ubyte2 pending_length = 0;
static ubyte1 *pending_data_ptr = NULL;

// ---------------- Helper Functions ----------------

// Validate address range
static bool is_valid_range(ubyte2 offset, ubyte2 length)
{
    // Check for overflow
    if (offset >= EEPROM_SIZE)
    {
        return FALSE;
    }
    
    // Check if operation would exceed EEPROM size
    if ((ubyte4)offset + (ubyte4)length > EEPROM_SIZE)
    {
        return FALSE;
    }
    
    return TRUE;
}

// ---------------- Main Functions ----------------

/**
 * Initialization of the EEPROM driver
 */
IO_ErrorType IO_EEPROM_Init(void)
{
    // Check if already initialized
    if (eeprom_initialized)
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    // Initialize EEPROM memory to 0xFF (erased state)
    memset(eeprom_memory, 0xFF, EEPROM_SIZE);
    
    // Set initialized flag
    eeprom_initialized = TRUE;
    eeprom_state = EEPROM_STATE_IDLE;
    
    pending_offset = 0;
    pending_length = 0;
    pending_data_ptr = NULL;
    
    return IO_E_OK;
}

/**
 * Deinitializes the EEPROM driver
 */
IO_ErrorType IO_EEPROM_DeInit(void)
{
    // Check if initialized
    if (!eeprom_initialized)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Reset state
    eeprom_initialized = FALSE;
    eeprom_state = EEPROM_STATE_IDLE;
    
    pending_offset = 0;
    pending_length = 0;
    pending_data_ptr = NULL;
    
    return IO_E_OK;
}

/**
 * Read data from the EEPROM
 */
IO_ErrorType IO_EEPROM_Read(ubyte2 offset, ubyte2 length, ubyte1 * const data)
{
    // Check if initialized
    if (!eeprom_initialized)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Validate pointer
    if (data == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Check if busy
    if (eeprom_state != EEPROM_STATE_IDLE)
    {
        return IO_E_BUSY;
    }
    
    // Validate range
    if (!is_valid_range(offset, length))
    {
        return IO_E_EEPROM_RANGE;
    }
    
    // In SIL, we can perform the read immediately
    // In real hardware, this would be asynchronous over SPI
    memcpy(data, &eeprom_memory[offset], length);
    
    // Optionally simulate busy state (uncomment to simulate asynchronous operation)
    // eeprom_state = EEPROM_STATE_BUSY_READ;
    // pending_offset = offset;
    // pending_length = length;
    // pending_data_ptr = data;
    
    // For simplicity, complete immediately in SIL
    eeprom_state = EEPROM_STATE_IDLE;
    
    return IO_E_OK;
}

/**
 * Write data to the EEPROM
 */
IO_ErrorType IO_EEPROM_Write(ubyte2 offset, ubyte2 length, const ubyte1 * const data)
{
    // Check if initialized
    if (!eeprom_initialized)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Validate pointer
    if (data == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Check if busy
    if (eeprom_state != EEPROM_STATE_IDLE)
    {
        return IO_E_BUSY;
    }
    
    // Validate range
    if (!is_valid_range(offset, length))
    {
        return IO_E_EEPROM_RANGE;
    }
    
    // In SIL, we can perform the write immediately
    // In real hardware, this would be asynchronous over SPI
    memcpy(&eeprom_memory[offset], data, length);
    
    // Optionally simulate busy state (uncomment to simulate asynchronous operation)
    // eeprom_state = EEPROM_STATE_BUSY_WRITE;
    // pending_offset = offset;
    // pending_length = length;
    // pending_data_ptr = (ubyte1*)data; // Cast away const for tracking
    
    // For simplicity, complete immediately in SIL
    eeprom_state = EEPROM_STATE_IDLE;
    
    return IO_E_OK;
}

/**
 * Returns the status of the EEPROM driver
 */
IO_ErrorType IO_EEPROM_GetStatus(void)
{
    // Check if initialized
    if (!eeprom_initialized)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Return busy state
    if (eeprom_state != EEPROM_STATE_IDLE)
    {
        return IO_E_BUSY;
    }
    
    return IO_E_OK;
}

// ---------------- Helper Functions for Testing ----------------

/**
 * SIL-specific function to simulate completing an async operation
 * This function is not part of the real IO_EEPROM API
 */
void IO_EEPROM_SIL_CompleteOperation(void)
{
    if (eeprom_state == EEPROM_STATE_BUSY_READ && pending_data_ptr != NULL)
    {
        // Complete the read operation
        memcpy(pending_data_ptr, &eeprom_memory[pending_offset], pending_length);
    }
    else if (eeprom_state == EEPROM_STATE_BUSY_WRITE && pending_data_ptr != NULL)
    {
        // Complete the write operation
        memcpy(&eeprom_memory[pending_offset], pending_data_ptr, pending_length);
    }
    
    // Mark as idle
    eeprom_state = EEPROM_STATE_IDLE;
    pending_offset = 0;
    pending_length = 0;
    pending_data_ptr = NULL;
}

/**
 * SIL-specific function to directly read EEPROM memory for verification
 * This function is not part of the real IO_EEPROM API
 */
IO_ErrorType IO_EEPROM_SIL_DirectRead(ubyte2 offset, ubyte2 length, ubyte1 * const data)
{
    // Validate pointer
    if (data == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Validate range
    if (!is_valid_range(offset, length))
    {
        return IO_E_EEPROM_RANGE;
    }
    
    // Direct read from memory (bypasses busy check)
    memcpy(data, &eeprom_memory[offset], length);
    
    return IO_E_OK;
}

/**
 * SIL-specific function to directly write EEPROM memory for setup
 * This function is not part of the real IO_EEPROM API
 */
IO_ErrorType IO_EEPROM_SIL_DirectWrite(ubyte2 offset, ubyte2 length, const ubyte1 * const data)
{
    // Validate pointer
    if (data == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Validate range
    if (!is_valid_range(offset, length))
    {
        return IO_E_EEPROM_RANGE;
    }
    
    // Direct write to memory (bypasses busy check and initialization check)
    memcpy(&eeprom_memory[offset], data, length);
    
    return IO_E_OK;
}

/**
 * SIL-specific function to clear all EEPROM memory
 * This function is not part of the real IO_EEPROM API
 */
void IO_EEPROM_SIL_ClearAll(void)
{
    memset(eeprom_memory, 0xFF, EEPROM_SIZE);
}

/**
 * SIL-specific function to save EEPROM to a file (for persistence between runs)
 * This function is not part of the real IO_EEPROM API
 */
IO_ErrorType IO_EEPROM_SIL_SaveToFile(const char *filename)
{
    if (filename == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // This would require file I/O which we're not implementing here
    // But the structure is in place for testing frameworks to use it
    
    return IO_E_OK;
}

/**
 * SIL-specific function to load EEPROM from a file (for persistence between runs)
 * This function is not part of the real IO_EEPROM API
 */
IO_ErrorType IO_EEPROM_SIL_LoadFromFile(const char *filename)
{
    if (filename == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // This would require file I/O which we're not implementing here
    // But the structure is in place for testing frameworks to use it
    
    return IO_E_OK;
}

