# VCU Bench-Test Scripts

## `dry_bench_sim.py`  -- Phase 1 "Dry Bench" CAN simulator

Spoofs the CAN traffic the VCU expects from the Cascadia MCM
(`0xA2`, `0xA5`, `0xA7`, `0xAA`) and listens for the `0xC0` motor
torque command that the VCU emits in response.

### Install

```bash
pip install -r test/requirements.txt
```

### Quick smoke test (no hardware, Mac or Linux)

```bash
python3 test/dry_bench_sim.py --interface virtual
```

The `virtual` backend is an in-memory loopback bus: the sim broadcasts
frames and reads them back itself, so you'll see all the spoofed
outbound traffic but no `0xC0` response (the VCU isn't on the bus).
Useful for verifying the script runs before moving to the bench.

### Real bench (Windows + Peak PCAN-USB)

```powershell
python test/dry_bench_sim.py --interface pcan --channel PCAN_USBBUS1 --bitrate 500000
```

### Real bench (Linux + socketcan, e.g. Kvaser / vcan0)

```bash
python3 test/dry_bench_sim.py --interface socketcan --channel can0 --bitrate 500000
```

### What it tests

| Test                        | How it's driven                                  | Watch for                     |
|-----------------------------|--------------------------------------------------|-------------------------------|
| 80 kW power ceiling         | Spoof `0xA5` motor_rpm = 6000                    | `0xC0` torque clamps at ~126.5 Nm (79.5 kW / 628 rad/s)  |
| MCM state machine gating    | Spoof `0xAA` byte6 = inv_enabled + lockout_clear | `0xC0` inv_en byte goes to 1  |
| Thermal derating pass-through | Spoof `0xA2` motor_temp = 45 C (below 75 C)     | `0xC0` torque unmodified       |
| `0xC0` transmit cadence     | Just listen                                       | ~100 Hz (every 10 ms)         |

### What it **cannot** test

These sensors are wired to HY-TTC 50 pins, **not** CAN, so spoofing
them over the bus is physically impossible:

* **Wheel speeds** (hall sensors on PWD pins) -> traction-control test
  requires a HIL rig that injects differential pulse trains.
* **APPS / BPS** (analog pots on ADC pins) -> full-throttle / brake
  tests require a HIL rig or a physical pedal box.
* **Steering angle sensor** (analog) -> same story for DRS-close tests.

The script broadcasts "ghost" wheel-speed frames on `0x503` / `0x504`
so your DAQ / logger has something to look at, but those frames are
VCU *output* IDs -- the VCU itself will not read them.
