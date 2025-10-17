#include <string.h>
#include "IO_CAN.h"
#include "IO_Constants.h"

typedef unsigned char ubyte1;
typedef unsigned int ubyte2;
typedef unsigned long ubyte4;

// ---------------- Configuration ----------------

#define MAX_CAN_CHANNELS 4
#define MAX_HANDLES 256
#define MAX_FIFO_SIZE 256

// CAN channel state
typedef struct {
    bool initialized;
    ubyte2 baudrate;
    ubyte1 tseg1;
    ubyte1 tseg2;
    ubyte1 sjw;
    ubyte1 rx_error_counter;
    ubyte1 tx_error_counter;
} CAN_Channel_State;

// Handle types
typedef enum {
    HANDLE_TYPE_NONE = 0,
    HANDLE_TYPE_MSG_READ,
    HANDLE_TYPE_MSG_WRITE,
    HANDLE_TYPE_FIFO_READ,
    HANDLE_TYPE_FIFO_WRITE
} Handle_Type;

// Message object state
typedef struct {
    bool allocated;
    Handle_Type type;
    ubyte1 channel;
    ubyte1 id_format;
    ubyte4 id;
    ubyte4 ac_mask;
    
    // For standard messages
    IO_CAN_DATA_FRAME msg_buffer;
    bool has_new_data;
    bool overflow;
    bool tx_busy;
    
    // For FIFO buffers
    IO_CAN_DATA_FRAME fifo_buffer[MAX_FIFO_SIZE];
    ubyte1 fifo_size;
    ubyte1 fifo_head;
    ubyte1 fifo_tail;
    ubyte1 fifo_count;
    bool fifo_overflow;
} Handle_State;

// Global state
static CAN_Channel_State channel_states[MAX_CAN_CHANNELS];
static Handle_State handle_states[MAX_HANDLES];

// ---------------- Helper Functions ----------------

// Validate CAN channel
static bool is_valid_channel(ubyte1 channel)
{
    if (channel == IO_CAN_CHANNEL_0) return TRUE;
    if (channel == IO_CAN_CHANNEL_1) return TRUE;
#if (defined(TTC94) || defined(TTC94E) || defined(TTC94R))
    if (channel == IO_CAN_CHANNEL_2) return TRUE;
    if (channel == IO_CAN_CHANNEL_3) return TRUE;
#endif
    return FALSE;
}

// Convert channel to index
static int channel_to_index(ubyte1 channel)
{
    if (channel == IO_CAN_CHANNEL_0) return 0;
    if (channel == IO_CAN_CHANNEL_1) return 1;
#if (defined(TTC94) || defined(TTC94E) || defined(TTC94R))
    if (channel == IO_CAN_CHANNEL_2) return 2;
    if (channel == IO_CAN_CHANNEL_3) return 3;
#endif
    return -1;
}

// Check if channel is initialized
static bool is_channel_initialized(ubyte1 channel)
{
    int idx = channel_to_index(channel);
    if (idx < 0) return FALSE;
    return channel_states[idx].initialized;
}

// Validate handle
static bool is_valid_handle(ubyte1 handle)
{
    return (handle_states[handle].allocated);
}

// Check if ID matches with acceptance mask
static bool id_matches(ubyte4 msg_id, ubyte4 filter_id, ubyte4 ac_mask)
{
    // Apply acceptance mask to both IDs and compare
    return ((msg_id & ac_mask) == (filter_id & ac_mask));
}

// Allocate a new handle
static IO_ErrorType allocate_handle(ubyte1 *handle, ubyte1 channel, Handle_Type type,
                                   ubyte1 id_format, ubyte4 id, ubyte4 ac_mask)
{
    // Find free handle
    for (ubyte2 i = 0; i < MAX_HANDLES; i++)
    {
        if (!handle_states[i].allocated)
        {
            // Allocate and initialize handle
            memset(&handle_states[i], 0, sizeof(Handle_State));
            handle_states[i].allocated = TRUE;
            handle_states[i].type = type;
            handle_states[i].channel = channel;
            handle_states[i].id_format = id_format;
            handle_states[i].id = id;
            handle_states[i].ac_mask = ac_mask;
            *handle = (ubyte1)i;
            return IO_E_OK;
        }
    }
    
    return IO_E_CAN_MAX_HANDLES_REACHED;
}

// ---------------- Main Functions ----------------

/**
 * Initialization of the CAN communication driver
 */
IO_ErrorType IO_CAN_Init(ubyte1 channel, ubyte2 baudrate, ubyte1 tseg1, 
                        ubyte1 tseg2, ubyte1 sjw)
{
    // Validate channel
    if (!is_valid_channel(channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Validate baudrate (125-1000 kbit/s)
    if (baudrate < 125 || baudrate > 1000)
    {
        // Allow 0 for default, but otherwise must be in range
        if (baudrate != 0)
        {
            return IO_E_INVALID_PARAMETER;
        }
        baudrate = 500; // Default to 500 kbit/s
    }
    
    // Validate timing parameters (0 means use defaults)
    if (tseg1 != 0 && (tseg1 < 3 || tseg1 > 16))
    {
        return IO_E_INVALID_PARAMETER;
    }
    if (tseg2 != 0 && (tseg2 < 2 || tseg2 > 8))
    {
        return IO_E_INVALID_PARAMETER;
    }
    if (sjw != 0 && (sjw < 1 || sjw > 4))
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    // Set defaults if parameters are 0
    if (tseg1 == 0) tseg1 = 10;
    if (tseg2 == 0) tseg2 = 5;
    if (sjw == 0) sjw = 2;
    
    // Check if already initialized
    int idx = channel_to_index(channel);
    if (channel_states[idx].initialized)
    {
        return IO_E_CHANNEL_BUSY;
    }
    
    // Initialize channel
    channel_states[idx].initialized = TRUE;
    channel_states[idx].baudrate = baudrate;
    channel_states[idx].tseg1 = tseg1;
    channel_states[idx].tseg2 = tseg2;
    channel_states[idx].sjw = sjw;
    channel_states[idx].rx_error_counter = 0;
    channel_states[idx].tx_error_counter = 0;
    
    return IO_E_OK;
}

/**
 * Deinitializes a single message handle
 */
IO_ErrorType IO_CAN_DeInitHandle(ubyte1 handle)
{
    // Validate handle
    if (!is_valid_handle(handle))
    {
        return IO_E_CAN_WRONG_HANDLE;
    }
    
    // Clear handle state
    memset(&handle_states[handle], 0, sizeof(Handle_State));
    
    return IO_E_OK;
}

/**
 * Deinitializes the given CAN channel
 */
IO_ErrorType IO_CAN_DeInit(ubyte1 channel)
{
    // Validate channel
    if (!is_valid_channel(channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if initialized
    if (!is_channel_initialized(channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Deinitialize all handles associated with this channel
    for (ubyte2 i = 0; i < MAX_HANDLES; i++)
    {
        if (handle_states[i].allocated && handle_states[i].channel == channel)
        {
            memset(&handle_states[i], 0, sizeof(Handle_State));
        }
    }
    
    // Clear channel state
    int idx = channel_to_index(channel);
    memset(&channel_states[idx], 0, sizeof(CAN_Channel_State));
    
    return IO_E_OK;
}

/**
 * Configures a message object
 */
IO_ErrorType IO_CAN_ConfigMsg(ubyte1 * const handle, ubyte1 channel, ubyte1 mode, 
                             ubyte1 id_format, ubyte4 id, ubyte4 ac_mask)
{
    // Validate pointer
    if (handle == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Validate channel
    if (!is_valid_channel(channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if channel is initialized
    if (!is_channel_initialized(channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Validate mode
    if (mode != IO_CAN_MSG_READ && mode != IO_CAN_MSG_WRITE)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    // Validate ID format
    if (id_format != IO_CAN_STD_FRAME && id_format != IO_CAN_EXT_FRAME)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    // Validate ID range based on format
    if (id_format == IO_CAN_STD_FRAME && id > 0x7FF)
    {
        return IO_E_INVALID_PARAMETER;
    }
    if (id_format == IO_CAN_EXT_FRAME && id > 0x1FFFFFFF)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    // Allocate handle
    Handle_Type type = (mode == IO_CAN_MSG_READ) ? HANDLE_TYPE_MSG_READ : HANDLE_TYPE_MSG_WRITE;
    return allocate_handle(handle, channel, type, id_format, id, ac_mask);
}

/**
 * Returns the data of a message object
 */
IO_ErrorType IO_CAN_ReadMsg(ubyte1 handle, IO_CAN_DATA_FRAME * const buffer)
{
    // Validate pointer
    if (buffer == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Validate handle
    if (!is_valid_handle(handle))
    {
        return IO_E_CAN_WRONG_HANDLE;
    }
    
    if (handle_states[handle].type != HANDLE_TYPE_MSG_READ)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    Handle_State *hs = &handle_states[handle];
    IO_ErrorType result = IO_E_OK;
    
    // Check for overflow
    if (hs->overflow)
    {
        result = IO_E_CAN_OVERFLOW;
        hs->overflow = FALSE;
    }
    
    // Check for new data
    if (!hs->has_new_data)
    {
        // Return old data with appropriate error code
        if (result == IO_E_OK)
        {
            result = IO_E_CAN_OLD_DATA;
        }
    }
    else
    {
        hs->has_new_data = FALSE;
    }
    
    // Copy message to buffer
    memcpy(buffer, &hs->msg_buffer, sizeof(IO_CAN_DATA_FRAME));
    
    return result;
}

/**
 * Transmits a CAN Message
 */
IO_ErrorType IO_CAN_WriteMsg(ubyte1 handle, const IO_CAN_DATA_FRAME * const data)
{
    // Validate pointer
    if (data == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Validate handle
    if (!is_valid_handle(handle))
    {
        return IO_E_CAN_WRONG_HANDLE;
    }
    
    if (handle_states[handle].type != HANDLE_TYPE_MSG_WRITE)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    Handle_State *hs = &handle_states[handle];
    
    // Check if busy
    if (hs->tx_busy)
    {
        return IO_E_BUSY;
    }
    
    // Validate data length
    if (data->length > 8)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    // Store message and mark as busy (simulated transmission)
    memcpy(&hs->msg_buffer, data, sizeof(IO_CAN_DATA_FRAME));
    hs->tx_busy = TRUE;
    
    // In SIL, we immediately mark as transmitted (no actual hardware delay)
    hs->tx_busy = FALSE;
    
    return IO_E_OK;
}

/**
 * Configures a FIFO buffer
 */
IO_ErrorType IO_CAN_ConfigFIFO(ubyte1 * const handle, ubyte1 channel, ubyte1 size, 
                              ubyte1 mode, ubyte1 id_format, ubyte4 id, ubyte4 ac_mask)
{
    // Validate pointer
    if (handle == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Validate channel
    if (!is_valid_channel(channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if channel is initialized
    if (!is_channel_initialized(channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    // Validate size (size is ubyte1, so max is 255)
    if (size == 0)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    // Validate mode
    if (mode != IO_CAN_MSG_READ && mode != IO_CAN_MSG_WRITE)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    // Validate ID format
    if (id_format != IO_CAN_STD_FRAME && id_format != IO_CAN_EXT_FRAME)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    // Validate ID range based on format
    if (id_format == IO_CAN_STD_FRAME && id > 0x7FF)
    {
        return IO_E_INVALID_PARAMETER;
    }
    if (id_format == IO_CAN_EXT_FRAME && id > 0x1FFFFFFF)
    {
        return IO_E_INVALID_PARAMETER;
    }
    
    // Allocate handle
    Handle_Type type = (mode == IO_CAN_MSG_READ) ? HANDLE_TYPE_FIFO_READ : HANDLE_TYPE_FIFO_WRITE;
    IO_ErrorType result = allocate_handle(handle, channel, type, id_format, id, ac_mask);
    
    if (result == IO_E_OK)
    {
        handle_states[*handle].fifo_size = size;
    }
    
    return result;
}

/**
 * Reads the data from a FIFO buffer
 */
IO_ErrorType IO_CAN_ReadFIFO(ubyte1 handle, IO_CAN_DATA_FRAME * const buffer, 
                            ubyte1 buffer_size, ubyte1 * const rx_frames)
{
    // Validate pointers
    if (buffer == NULL || rx_frames == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Validate handle
    if (!is_valid_handle(handle))
    {
        return IO_E_CAN_WRONG_HANDLE;
    }
    
    if (handle_states[handle].type != HANDLE_TYPE_FIFO_READ)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    Handle_State *hs = &handle_states[handle];
    IO_ErrorType result = IO_E_OK;
    
    // Check for overflow
    if (hs->fifo_overflow)
    {
        result = IO_E_CAN_FIFO_FULL;
        hs->fifo_overflow = FALSE;
    }
    
    // Check if FIFO is empty
    if (hs->fifo_count == 0)
    {
        *rx_frames = 0;
        if (result == IO_E_OK)
        {
            result = IO_E_CAN_OLD_DATA;
        }
        return result;
    }
    
    // Read frames from FIFO
    *rx_frames = 0;
    ubyte1 frames_to_read = (buffer_size < hs->fifo_count) ? buffer_size : hs->fifo_count;
    
    for (ubyte1 i = 0; i < frames_to_read; i++)
    {
        memcpy(&buffer[i], &hs->fifo_buffer[hs->fifo_tail], sizeof(IO_CAN_DATA_FRAME));
        hs->fifo_tail = (hs->fifo_tail + 1) % hs->fifo_size;
        hs->fifo_count--;
        (*rx_frames)++;
    }
    
    return result;
}

/**
 * Writes CAN frames to a FIFO buffer
 */
IO_ErrorType IO_CAN_WriteFIFO(ubyte1 handle, const IO_CAN_DATA_FRAME * const data, 
                             ubyte1 tx_length)
{
    // Validate pointer
    if (data == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Validate handle
    if (!is_valid_handle(handle))
    {
        return IO_E_CAN_WRONG_HANDLE;
    }
    
    if (handle_states[handle].type != HANDLE_TYPE_FIFO_WRITE)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    Handle_State *hs = &handle_states[handle];
    
    // Check if there's enough space
    ubyte1 free_space = hs->fifo_size - hs->fifo_count;
    if (tx_length > free_space)
    {
        return IO_E_CAN_FIFO_FULL;
    }
    
    // Write frames to FIFO
    for (ubyte1 i = 0; i < tx_length; i++)
    {
        // Validate data length
        if (data[i].length > 8)
        {
            return IO_E_INVALID_PARAMETER;
        }
        
        memcpy(&hs->fifo_buffer[hs->fifo_head], &data[i], sizeof(IO_CAN_DATA_FRAME));
        hs->fifo_head = (hs->fifo_head + 1) % hs->fifo_size;
        hs->fifo_count++;
    }
    
    // In SIL, we immediately mark as transmitted (no actual hardware delay)
    // Simulate transmission by clearing the FIFO
    hs->fifo_head = 0;
    hs->fifo_tail = 0;
    hs->fifo_count = 0;
    
    return IO_E_OK;
}

/**
 * Returns the error counters of the CAN channel
 */
IO_ErrorType IO_CAN_Status(ubyte1 channel, ubyte1 * const rx_error_counter, 
                          ubyte1 * const tx_error_counter)
{
    // Validate pointers
    if (rx_error_counter == NULL || tx_error_counter == NULL)
    {
        return IO_E_NULL_POINTER;
    }
    
    // Validate channel
    if (!is_valid_channel(channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    
    // Check if channel is initialized
    if (!is_channel_initialized(channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    int idx = channel_to_index(channel);
    *rx_error_counter = channel_states[idx].rx_error_counter;
    *tx_error_counter = channel_states[idx].tx_error_counter;
    
    // Check for error states (Bus Off at 255, Error Passive at 128)
    if (channel_states[idx].tx_error_counter >= 255)
    {
        return IO_E_CAN_BUS_OFF;
    }
    
    if (channel_states[idx].tx_error_counter >= 128 || 
        channel_states[idx].rx_error_counter >= 128)
    {
        return IO_E_CAN_ERROR_PASSIVE;
    }
    
    return IO_E_OK;
}

/**
 * Returns the status of a message buffer object
 */
IO_ErrorType IO_CAN_MsgStatus(ubyte1 handle)
{
    // Validate handle
    if (!is_valid_handle(handle))
    {
        return IO_E_CAN_WRONG_HANDLE;
    }
    
    if (handle_states[handle].type != HANDLE_TYPE_MSG_READ && 
        handle_states[handle].type != HANDLE_TYPE_MSG_WRITE)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    Handle_State *hs = &handle_states[handle];
    
    // Check for transmission busy
    if (hs->tx_busy)
    {
        return IO_E_BUSY;
    }
    
    // Check for overflow
    if (hs->overflow)
    {
        return IO_E_CAN_OVERFLOW;
    }
    
    // Check for new data (only for read handles)
    if (hs->type == HANDLE_TYPE_MSG_READ && !hs->has_new_data)
    {
        return IO_E_CAN_OLD_DATA;
    }
    
    return IO_E_OK;
}

/**
 * Returns the status of a FIFO buffer
 */
IO_ErrorType IO_CAN_FIFOStatus(ubyte1 handle)
{
    // Validate handle
    if (!is_valid_handle(handle))
    {
        return IO_E_CAN_WRONG_HANDLE;
    }
    
    if (handle_states[handle].type != HANDLE_TYPE_FIFO_READ && 
        handle_states[handle].type != HANDLE_TYPE_FIFO_WRITE)
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    
    Handle_State *hs = &handle_states[handle];
    
    // Check for overflow
    if (hs->fifo_overflow)
    {
        return IO_E_CAN_FIFO_FULL;
    }
    
    // Check if FIFO has data (for read) or is full (for write)
    if (hs->type == HANDLE_TYPE_FIFO_READ && hs->fifo_count == 0)
    {
        return IO_E_CAN_OLD_DATA;
    }
    
    if (hs->type == HANDLE_TYPE_FIFO_WRITE && hs->fifo_count >= hs->fifo_size)
    {
        return IO_E_CAN_FIFO_FULL;
    }
    
    // Check if transmission is ongoing
    if (hs->fifo_count > 0)
    {
        return IO_E_BUSY;
    }
    
    return IO_E_OK;
}

// ---------------- Helper Functions for Testing ----------------

/**
 * SIL-specific function to inject a CAN message for testing
 * This simulates receiving a message on a channel
 */
void IO_CAN_SIL_InjectMessage(ubyte1 channel, const IO_CAN_DATA_FRAME * const frame)
{
    if (frame == NULL || !is_valid_channel(channel))
    {
        return;
    }
    
    // Find all read handles matching this message and inject it
    for (ubyte2 i = 0; i < MAX_HANDLES; i++)
    {
        Handle_State *hs = &handle_states[i];
        
        if (!hs->allocated || hs->channel != channel)
        {
            continue;
        }
        
        // Check if ID matches with acceptance mask
        if (!id_matches(frame->id, hs->id, hs->ac_mask))
        {
            continue;
        }
        
        // Inject into standard message buffer
        if (hs->type == HANDLE_TYPE_MSG_READ)
        {
            if (hs->has_new_data)
            {
                hs->overflow = TRUE; // Message not read yet, overflow
            }
            memcpy(&hs->msg_buffer, frame, sizeof(IO_CAN_DATA_FRAME));
            hs->has_new_data = TRUE;
        }
        // Inject into FIFO buffer
        else if (hs->type == HANDLE_TYPE_FIFO_READ)
        {
            if (hs->fifo_count >= hs->fifo_size)
            {
                hs->fifo_overflow = TRUE; // FIFO full, overflow
                // Overwrite oldest message
                hs->fifo_tail = (hs->fifo_tail + 1) % hs->fifo_size;
                hs->fifo_count--;
            }
            
            memcpy(&hs->fifo_buffer[hs->fifo_head], frame, sizeof(IO_CAN_DATA_FRAME));
            hs->fifo_head = (hs->fifo_head + 1) % hs->fifo_size;
            hs->fifo_count++;
        }
    }
}

/**
 * SIL-specific function to set error counters for testing
 */
void IO_CAN_SIL_SetErrorCounters(ubyte1 channel, ubyte1 rx_errors, ubyte1 tx_errors)
{
    int idx = channel_to_index(channel);
    if (idx >= 0)
    {
        channel_states[idx].rx_error_counter = rx_errors;
        channel_states[idx].tx_error_counter = tx_errors;
    }
}

