#include "IO_UART.h"
#include "serial.h"
#include <stdio.h>
#include <assert.h>

// Small helper for readable asserts
static void expect_eq(const char* name, IO_ErrorType got, IO_ErrorType want) {
    if (got != want) {
        printf("[FAIL] %s: got %d, want %d\n", name, (int)got, (int)want);
    } else {
        printf("[ OK ] %s\n", name);
    }
    assert(got == want);
}

int main(void) {
    SerialManager* serialMan = SerialManager_new();
    /*
    IO_ErrorType err;

    // --- CH0 / RS232 basic success ---
    err = IO_UART_Init(IO_UART_RS232, 115200, 8, IO_UART_PARITY_NONE, 1);
    expect_eq("CH0 valid init (115200, 8N1)", err, IO_E_OK);

    // --- CH0 busy if re-inited without deinit ---
    err = IO_UART_Init(IO_UART_RS232, 115200, 8, IO_UART_PARITY_NONE, 1);
    expect_eq("CH0 re-init busy", err, IO_E_CHANNEL_BUSY);

    // Deinit then re-init should succeed
    err = IO_UART_DeInit(IO_UART_RS232);
    expect_eq("CH0 deinit", err, IO_E_OK);
    err = IO_UART_Init(IO_UART_RS232, 9600, 8, IO_UART_PARITY_NONE, 1);
    expect_eq("CH0 re-init after deinit", err, IO_E_OK);

    // --- CH0 invalid dbits/sbits/parity (non-LIN checks apply) ---
    err = IO_UART_DeInit(IO_UART_RS232);
    expect_eq("CH0 deinit before invalid tests", err, IO_E_OK);

    err = IO_UART_Init(IO_UART_RS232, 9600, 9, IO_UART_PARITY_NONE, 1); // dbits > 8
    expect_eq("CH0 invalid dbits=9", err, IO_E_INVALID_PARAMETER);

    err = IO_UART_Init(IO_UART_RS232, 9600, 8, IO_UART_PARITY_NONE, 3); // sbits > 2
    expect_eq("CH0 invalid sbits=3", err, IO_E_INVALID_PARAMETER);

    err = IO_UART_Init(IO_UART_RS232, 9600, 8, (ubyte1)0x1, 1); // invalid parity (valid: 0x0,0x2,0x3)
    expect_eq("CH0 invalid parity=0x1", err, IO_E_INVALID_PARAMETER);

    // --- CH1 / LIN: dbits/par/sbits are IGNORED, but baud range is stricter (<= 20000) ---
    err = IO_UART_Init(IO_UART_LIN, 19200, 5, IO_UART_PARITY_ODD, 2);  // weird format OK (ignored)
    expect_eq("CH1 LIN init (params ignored)", err, IO_E_OK);

    // Re-init without deinit -> busy
    err = IO_UART_Init(IO_UART_LIN, 19200, 8, IO_UART_PARITY_NONE, 1);
    expect_eq("CH1 LIN re-init busy", err, IO_E_CHANNEL_BUSY);

    // Deinit and test invalid LIN baud (> 20000)
    err = IO_UART_DeInit(IO_UART_LIN);
    expect_eq("CH1 LIN deinit", err, IO_E_OK);

    err = IO_UART_Init(IO_UART_LIN, 57600, 8, IO_UART_PARITY_NONE, 1); // too fast for LIN
    expect_eq("CH1 LIN invalid baud=57600", err, IO_E_INVALID_PARAMETER);

    // --- CH2 / MiniMod success ---
    err = IO_UART_Init(IO_UART_MiniMod, 115200, 8, IO_UART_PARITY_NONE, 1);
    expect_eq("CH2 valid init", err, IO_E_OK);

    // --- Invalid channel example (use a value you know is invalid) ---
    err = IO_UART_Init((ubyte1)99, 9600, 8, IO_UART_PARITY_NONE, 1);
    expect_eq("invalid channel=99", err, IO_E_INVALID_CHANNEL_ID);

    printf("\nAll tests passed.\n");
    return 0;
    */
}
