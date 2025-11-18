# Hardware-in-the-Loop (HIL) Testing Platform

## Overview
The HIL platform provides a modular testing infrastructure for designers, allowing them to physically monitor CAN traffic and validate different subsystem algorithms (MCM, LC, Regen...) implemented on VCU firmware.

## Current Setup

- **Host PC** runs test scripts
- **PCAN-USB Adapter** bridges the host PC and VCU communicaion
- **VCU** runs the firmware 

```
┌──────────────┐         ┌─────────────┐         ┌──────────────┐
│   Host PC    │   USB   │  PCAN-USB   │   CAN   │    TTTech    │
│ (Python HIL) │◄───────►│   Adapter   │◄───────►│   TTC50 VCU  │
│   Scripts    │         │             │         │  (Firmware)  │
└──────────────┘         └─────────────┘         └──────────────┘
```

## Requirements
- Python 3.7+
- PCAN-USB Drivers

### Installation

1. **Install PCAN-USB drivers**
    - Download drivers [here](https://www.peak-system.com/PCAN-USB.199.0.html?L=1) for your OS

2. **Set up virtual environment and install dependencies**

    ```bash
    cd testing/hil/

    # Create virtual environment
    python3 -m venv venv

    # Activate virtual environment
    source venv/bin/activate    # On macOS/Linux
    # or
    venv\Scripts\activate       # On Windows

    # Install dependencies
    pip install -r requirements.txt

    # Install HIL CLI tool in development mode
    pip install -e .
    ```

3. **Verify installation**
    ```bash
    # Check CLI is installed
    hil --version
    # Expected: sre-hil, version 0.1.0

    # Check PCAN hardware (if connected)
    hil listen --duration 5
    # or use legacy script:
    python scripts/utils/pc_pcan.py
    ```

### Configuration

To discover the PCAN-USB device and interface with it, a `can.ini` config file in `config/` is provided (can be edited if needed):

```ini
[default]
interface = pcan
channel = PCAN_USBBUS1    # Change if using different channel
bitrate = 500000          # 500 kbps (must match VCU)
```

For **macOS users**, define a `.canrc` file with the same content inside your macOS user directory (`/Users/<user>`).

## CLI Usage

The HIL platform now includes a comprehensive CLI tool (`hil`) for CAN testing:

### Basic Commands

```bash
# Show help
hil --help

# Listen to raw CAN traffic
hil listen --duration 10

# Listen with DBC decoding
hil listen --decode

# Filter by CAN ID
hil listen --filter 0x50B --decode

# Filter by message name
hil listen --message VCU_LC_Status_A --decode

# Override DBC file
hil --dbc dbcs/10.03.25_LC_Main.dbc listen --decode
```

### Available Commands

- `hil listen` - Listen to CAN bus traffic (BUSMASTER monitoring equivalent)
- `hil version` - Show version information

**Coming Soon:**
- `hil signal-watch` - Real-time signal monitoring with live table
- `hil inject` - Inject CAN messages
- `hil transmit --interactive` - Interactive message builder

## Running tests

As of now, the platform is set up with a couple of Python test scripts (located in `scripts/`) that do the following:
- Test PCAN connection
- Listen to VCU CAN bus
- Subsystem CAN testing scenarios (e.g., Launch Control slip ratio calculation)

***---More testing scenarios and capabilities to come---***

## Current test script template

```python
import time
import can
import cantools

# Configuration
TX_PERIOD_S = 0.01              # Update rate (Hz)
TEST_DURATION_SECONDS = 5.0     # Test duration

# Load DBC file (path relative to testing/hil/)
dbc = cantools.database.load_file("dbcs/10.22.25_SRE_Main.dbc", strict=False)

# Get CAN messages
tx_msg = dbc.get_message_by_name("Your_TX_Message_Name")
rx_msg = dbc.get_message_by_name("Your_RX_Message_Name")

# Initialize CAN bus (uses config/can.ini)
bus = can.interface.Bus()

print("Starting test...")

t_end = time.time() + TEST_DURATION_SECONDS
next_tx = 0.0

try:
    while time.time() < t_end:
        now = time.time()

        # Transmit at specified rate
        if now >= next_tx:
            data = tx_msg.encode({"Signal_Name": value})
            bus.send(can.Message(arbitration_id=tx_msg.frame_id,
                                data=data,
                                is_extended_id=False))
            next_tx = now + TX_PERIOD_S

        # Receive and process
        rx = bus.recv(0.001)
        if rx and rx.arbitration_id == rx_msg.frame_id:
            decoded = rx_msg.decode(bytes(rx.data))
            print(f"Received: {decoded}")

    print("Done.")

finally:
    bus.shutdown()
```