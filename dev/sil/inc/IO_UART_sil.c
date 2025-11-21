#include <string.h>
#include <stdio.h>
#include "IO_UART.h"
#include "IO_Driver.h"

typedef unsigned char ubyt1;
typedef unsigned long ubyte4;

// ---------------- Configuration ----------------
ubyte1 channels[] = {
    IO_UART_RS232,
    IO_UART_LIN,
    IO_UART_MiniMod};

ubyte1 parity[] = {
    IO_UART_PARITY_NONE,
    IO_UART_PARITY_EVEN,
    IO_UART_PARITY_ODD};

static bool initialized_channels[sizeof(channels)] = {FALSE, FALSE, FALSE};

// ---------------- Helper Functions----------------
// checks for valid channel
bool isValidChannel(ubyte1 channel)
{
    for (ubyte1 i = 0; i < sizeof(channels); i++)
    {
        if (channels[i] == channel)
        {
            return TRUE;
        }
    }
    return FALSE;
}

// checks for valid baudrate based on channel
bool is_valid_baudrate(ubyte1 channel, ubyte4 baudrate)
{
    switch (channel)
    {
    case IO_UART_RS232:
        return (baudrate >= 1200 && baudrate <= 115200);

    case IO_UART_LIN:
        return (baudrate >= 1200 && baudrate <= 20000);

    case IO_UART_MiniMod:
        return (baudrate >= 1200 && baudrate <= 115200);

    default:
        return FALSE;
    }
}

// checks if parity is valid
bool isValidParity(ubyte1 par)
{
    for (ubyte1 i = 0; i < sizeof(parity); i++)
    {
        if (parity[i] == par)
        {
            return TRUE;
        }
    }
    return FALSE;
}

// returns index of channel in channels array
static int channel_id(ubyte1 channel)
{
    switch (channel)
    {
    case IO_UART_RS232:
        return 0;
    case IO_UART_LIN:
        return 1;
    case IO_UART_MiniMod:
        return 2;
    default:
        return -1; // invalid
    }
}

// checks if channel is initialized
bool is_channel_initialized(ubyte1 channel)
{
    int id = channel_id(channel);
    if (id == -1)
    {
        return FALSE; // invalid channel
    }
    return initialized_channels[id];
}


// ---------------- Main Functions----------------
/*   Initialization of UART Serial Communication Driver
 *      - Enables module
 *      - Configures module for ASC
 *      - Initializes SW queue
*/
//SW QUEUE AND HW BUFFER NOT IMPLEMENTED YET
IO_ErrorType IO_UART_Init(ubyte1 channel, ubyte4 baudrate, ubyte1 dbits, ubyte1 par, ubyte1 sbits)
{
    if (!isValidChannel(channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    if (!is_valid_baudrate(channel, baudrate))
    {
        return IO_E_INVALID_PARAMETER;
    }

    /*When LIN is selected (i.e. channel = IO_UART_LIN), the parameters
     *dbits, par and sbits will be ignored. LIN always uses 1 stop bit
     *no parity and 8 data bits (except for the break frame).
     */
    bool is_lin = (channel == IO_UART_LIN);
    if (!is_lin)
    {
        if (dbits < 1 || dbits > 8)
        {
            return IO_E_INVALID_PARAMETER;
        }
        if (sbits < 1 || sbits > 2)
        {
            return IO_E_INVALID_PARAMETER;
        }
        if (!isValidParity(par))
        {
            return IO_E_INVALID_PARAMETER;
        }
    }

    if(is_lin) {
        dbits = 8;
        par = IO_UART_PARITY_NONE;
        sbits = 1;
    }

    if (is_channel_initialized(channel))
    {
        return IO_E_CHANNEL_BUSY;
    }
    int id = channel_id(channel);
    initialized_channels[id] = TRUE;
    return IO_E_OK;
}

/* Deinitialization of the UART channel */
IO_ErrorType IO_UART_DeInit(ubyte1 channel)
{
    if (!isValidChannel(channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    if (!is_channel_initialized(channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    int id = channel_id(channel);
    initialized_channels[id] = FALSE;
    return IO_E_OK;
}

/* Task function for UART communication [PLACEHOLDER] */
IO_ErrorType IO_UART_Task(void)
{
    return IO_E_OK; //placeholder
}

/* Read data from serial interface [PLACEHOLDER] */
IO_ErrorType IO_UART_Read(ubyte1 channel, ubyte1 *const data, ubyte1 len, ubyte1 *const rx_len)
{
    if (!isValidChannel(channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    if (!is_channel_initialized(channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    if (data == NULL || rx_len == NULL)
    {
        return IO_E_NULL_POINTER;
    }

    //Placeholder implementation
    *rx_len = 0;
    return IO_E_OK;
}

/* Write data to serial interface [PLACEHOLDER] */
IO_ErrorType IO_UART_Write(ubyte1 channel, const ubyte1 *const data, ubyte1 len, ubyte1 *const tx_len)
{
    if (!isValidChannel(channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    if (!is_channel_initialized(channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    if (data == NULL || tx_len == NULL)
    {
        return IO_E_NULL_POINTER;
    }

    //Placeholder implementation
    *tx_len = len;
    return IO_E_OK;
}

/* Retrieve the status of the receive buffer [PLACEHOLDER] */
IO_ErrorType IO_UART_GetRxStatus(ubyte1 channel, ubyte1 *const rx_len)
{
    if (!isValidChannel(channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    if (!is_channel_initialized(channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    if (rx_len == NULL)
    {
        return IO_E_NULL_POINTER;
    }

    //Placeholder implementation
    *rx_len = 0;
    return IO_E_OK;
}

/* Retrieve the status of the transmit buffer [PLACEHOLDER] */
IO_ErrorType IO_UART_GetTxStatus(ubyte1 channel, ubyte1 *const tx_len)
{
    if (!isValidChannel(channel))
    {
        return IO_E_INVALID_CHANNEL_ID;
    }
    if (!is_channel_initialized(channel))
    {
        return IO_E_CHANNEL_NOT_CONFIGURED;
    }
    if (tx_len == NULL)
    {
        return IO_E_NULL_POINTER;
    }

    //Placeholder implementation
    *tx_len = 0;
    return IO_E_OK;
}