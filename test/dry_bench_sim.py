#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
dry_bench_sim.py  -  Phase 1 "Dry Bench" VCU simulator.

Purpose
-------
Exercise the VCU's CAN-reachable state machines (motor RPM, MCM status)
and watch the 0xC0 torque command it emits in response. Used as a
bench-test rig before the car is connected to the inverter.

Scope / Honest limitations
--------------------------
The HY-TTC 50 VCU reads most driver inputs directly off analog / PWD pins,
NOT off the CAN bus:

    * APPS  (TPS0, TPS1)  -> ADC pins (via IO_ADC_* in torqueEncoder.c)
    * BPS0 / BPS1         -> ADC pins
    * Wheel-speed hall    -> PWD pins  (via IO_PWD_* in wheelSpeeds.c)
    * Steering angle      -> analog input

None of these can be spoofed over CAN. Full end-to-end traction-control /
APPS / 80 kW validation therefore requires a HIL (Hardware-In-the-Loop)
box that can drive voltages / pulse-trains onto those pins -- a CAN sim
alone cannot do it.

What this script DOES do
------------------------
1. Spoof motor RPM = 6000 on 0xA5 (the MCM's own broadcast ID).
   -> drives the 80 kW power-limit math in safety.c:
        tMaxDNm = (79500 W / omega) * 10
   ..so any torque command above that ceiling will be clamped. Driver
   throttle for the 80 kW test still needs to be asserted on the APPS
   pins (via HIL rig or pedal), but once throttle is asserted the RPM
   is what unlocks the power-limit code path.

2. Spoof MCM inverter-enabled / lockout-cleared on 0xAA so the VCU will
   emit a non-zero torque command (otherwise MCM_calculateCommands
   holds at 0 for safety).

3. Spoof MCM DC bus voltage on 0xA7 (400 V) so the power math has
   realistic numbers for logging.

4. Broadcast "ghost" wheel-speed frames on 0x503 / 0x504 that match the
   VCU's own WSS telemetry layout. These do NOT change what the VCU
   reads from its hall sensors, but they give your data logger
   something to display alongside the real VCU output, and they let
   you verify your CAN tooling is decoding the WSS IDs correctly.
   >> For the real rear-slip traction-control test you MUST use a HIL
      box to drive differential pulse rates into the PWD pins on the
      VCU. This is called out in the README.

5. Listen on 0xC0 and decode byte 0, byte 1 as a little-endian signed
   16-bit value in deci-Newton-meters (dNm). Prints Nm in real-time.

Message layout reference (from dev/canManager.c :: canOutput_sendMCMCommand)
---------------------------------------------------------------------------
    ID  0xC0   VCU -> Cascadia MCM torque command
    byte 0   torque        LSB (sbyte2, dNm)
    byte 1   torque        MSB
    byte 2   speed (RPM)   LSB (unused in torque mode)
    byte 3   speed (RPM)   MSB
    byte 4   direction     (0 = FWD, 1 = REV)
    byte 5   inverter_enable (0/1)
    byte 6   torqueLimit   LSB (sbyte2, dNm)
    byte 7   torqueLimit   MSB

Usage
-----
    # On a Mac / Linux dev box with no hardware (sanity test only):
    python3 dry_bench_sim.py --interface virtual

    # With a Peak PCAN-USB on Windows:
    python3 dry_bench_sim.py --interface pcan --channel PCAN_USBBUS1 --bitrate 500000

    # On Linux with a kvaser / socketcan:
    python3 dry_bench_sim.py --interface socketcan --channel can0 --bitrate 500000

    Ctrl+C to shut down cleanly.

Dependencies
------------
    python-can >= 4.0   (see test/requirements.txt)
"""

from __future__ import annotations

import argparse
import logging
import signal
import struct
import sys
import threading
import time
from dataclasses import dataclass
from typing import Optional

try:
    import can
except ImportError:
    sys.stderr.write(
        "\nERROR: python-can is not installed.\n"
        "  pip install -r test/requirements.txt\n"
        "  or: pip install python-can\n\n"
    )
    sys.exit(1)


# =============================================================================
# CAN IDs  (kept in sync with dev/canManager.c and dev/motorController.c)
# =============================================================================
# --- Inbound to VCU (we spoof these as if we were the Cascadia MCM) ---------
ID_MCM_TEMPS_B         = 0x0A2   # byte 4,5 = motor_temp * 10
ID_MCM_MOTOR_SPEED     = 0x0A5   # byte 2,3 = motor RPM (u16 LE)
ID_MCM_DC_BUS          = 0x0A7   # byte 0,1 = DC voltage * 10
ID_MCM_STATUS          = 0x0AA   # byte 6   = bit0 inverter_enabled, bit7 lockout
ID_MCM_CMD_TORQUE_FB   = 0x0AC   # byte 0,1 = commanded torque (feedback)

# --- Ghost wheel-speed broadcasts (see "Honest limitations" above) ----------
ID_WSS_MMPS            = 0x503   # FL,FR,RL,RR mm/s (u16 LE, 2B each)
ID_WSS_RPM_NONINTERP   = 0x504   # FL,FR,RL,RR RPM   (u16 LE, 2B each)

# --- Virtual HIL sensor override (VCU <- sim, only honored if bench mode) ---
# Paired with dev/virtualSensors.c :: VS_parseOverrideCanMessage()
# Byte layout (8B, STD):
#   0        APPS percent     u8  0..100         -> /100 float
#   1        BPS  percent     u8  0..100         -> /100 float
#   2,3      front wheels     u16 LE, kph*10     -> 0.1 kph res
#   4,5      rear  wheels     u16 LE, kph*10
#   6,7      steering angle   s16 LE, degrees
ID_BENCH_OVERRIDE      = 0x5FE

# --- VCU output we LISTEN for ----------------------------------------------
ID_MCM_TORQUE_CMD      = 0x0C0   # VCU -> MCM torque command (the money shot)


# =============================================================================
# Scenario setpoints (edit here to drive different tests)
# =============================================================================
@dataclass
class Scenario:
    # Traction-control scenario -- ALSO fed directly into the VCU via the
    # 0x5FE Virtual HIL override (as long as the VCU booted with IO_DI_06
    # pulled low for bench mode).
    front_wheel_kph: float = 10.0       # front axle ground speed
    rear_wheel_kph:  float = 20.0       # rear  axle ground speed (slip!)

    # Pedal / steering commanded via Virtual HIL (0x5FE)
    apps_percent:    int   = 100        # 0..100 integer %
    bps_percent:     int   = 0          # 0..100 integer %
    steering_deg:    int   = 0          # -32768..32767

    # 80 kW-ceiling scenario
    motor_rpm:       int   = 6000       # triggers power ceiling calc
    dc_bus_volts:    float = 400.0      # realistic HV for logging
    motor_temp_c:    float = 45.0       # well below derating threshold (75 C)

    # MCM status we feed back so VCU will emit non-zero torque
    inverter_enabled: bool = True
    lockout_cleared:  bool = True

    # Broadcast periods (seconds). The MCM really broadcasts ~3 ms
    # for the hot IDs; 20 ms is plenty for bench-testing.
    period_mcm_hot_s:  float = 0.020
    period_wss_s:      float = 0.010    # match the VCU's 10 ms loop cadence
    period_bench_s:    float = 0.010    # 100 Hz override stream


SCEN = Scenario()


# =============================================================================
# Packing helpers
# =============================================================================
def _kph_to_mmps(kph: float) -> int:
    """Convert km/h to mm/s (same unit as the VCU's WSS mm/s telemetry)."""
    return int(round(kph * 1000.0 * 1000.0 / 3600.0))  # km/h -> mm/s


def _kph_to_rpm(kph: float, tire_circ_m: float = 1.395) -> int:
    """Rough ground-speed -> wheel RPM (tire circumference 1.395 m per
    motorController.c). Good enough to give the logger a plausible number."""
    if tire_circ_m <= 0.0:
        return 0
    mps = kph * 1000.0 / 3600.0
    rev_per_sec = mps / tire_circ_m
    return int(round(rev_per_sec * 60.0))


def build_frame_a5_motor_rpm(rpm: int) -> can.Message:
    """0xA5: motor speed in bytes 2,3 (u16 LE)."""
    rpm_clamped = max(0, min(65535, int(rpm)))
    data = bytearray(8)
    data[2] = rpm_clamped & 0xFF
    data[3] = (rpm_clamped >> 8) & 0xFF
    return can.Message(arbitration_id=ID_MCM_MOTOR_SPEED,
                       data=data, is_extended_id=False)


def build_frame_a2_motor_temp(temp_c: float) -> can.Message:
    """0xA2: motor temperature in bytes 4,5 (value/10 degC)."""
    raw = max(0, min(65535, int(round(temp_c * 10.0))))
    data = bytearray(8)
    data[4] = raw & 0xFF
    data[5] = (raw >> 8) & 0xFF
    return can.Message(arbitration_id=ID_MCM_TEMPS_B,
                       data=data, is_extended_id=False)


def build_frame_a7_dc_bus(volts: float) -> can.Message:
    """0xA7: DC bus voltage in bytes 0,1 (value/10 V)."""
    raw = max(0, min(65535, int(round(volts * 10.0))))
    data = bytearray(8)
    data[0] = raw & 0xFF
    data[1] = (raw >> 8) & 0xFF
    return can.Message(arbitration_id=ID_MCM_DC_BUS,
                       data=data, is_extended_id=False)


def build_frame_aa_status(inverter_enabled: bool,
                          lockout_cleared: bool) -> can.Message:
    """0xAA: internal states byte 6. bit0=inverter enabled, bit7=lockout."""
    flags = 0
    if inverter_enabled:
        flags |= 0x01           # bit 0
    if not lockout_cleared:
        flags |= 0x80           # bit 7 set means LOCKOUT ACTIVE; clear=0
    data = bytearray(8)
    data[6] = flags
    return can.Message(arbitration_id=ID_MCM_STATUS,
                       data=data, is_extended_id=False)


def build_frame_wss_mmps(front_kph: float, rear_kph: float) -> can.Message:
    """0x503: FL, FR, RL, RR wheel speeds in mm/s. u16 LE each."""
    fl = _kph_to_mmps(front_kph)
    fr = _kph_to_mmps(front_kph)
    rl = _kph_to_mmps(rear_kph)
    rr = _kph_to_mmps(rear_kph)
    data = struct.pack('<HHHH', fl & 0xFFFF, fr & 0xFFFF,
                                rl & 0xFFFF, rr & 0xFFFF)
    return can.Message(arbitration_id=ID_WSS_MMPS,
                       data=data, is_extended_id=False)


def build_frame_wss_rpm(front_kph: float, rear_kph: float) -> can.Message:
    """0x504: FL, FR, RL, RR wheel speeds in RPM. u16 LE each."""
    fl = _kph_to_rpm(front_kph)
    fr = _kph_to_rpm(front_kph)
    rl = _kph_to_rpm(rear_kph)
    rr = _kph_to_rpm(rear_kph)
    data = struct.pack('<HHHH', fl & 0xFFFF, fr & 0xFFFF,
                                rl & 0xFFFF, rr & 0xFFFF)
    return can.Message(arbitration_id=ID_WSS_RPM_NONINTERP,
                       data=data, is_extended_id=False)


def build_frame_bench_override(apps_pct: int,
                               bps_pct: int,
                               front_kph: float,
                               rear_kph: float,
                               steering_deg: int) -> can.Message:
    """0x5FE: Virtual HIL sensor override. Must match byte-for-byte with
    dev/virtualSensors.c :: VS_parseOverrideCanMessage().

        byte 0   APPS percent     u8, 0..100
        byte 1   BPS percent      u8, 0..100
        byte 2,3 front wheels     u16 LE, kph * 10
        byte 4,5 rear  wheels     u16 LE, kph * 10
        byte 6,7 steering deg     s16 LE
    """
    apps_b = max(0, min(100, int(apps_pct)))
    bps_b  = max(0, min(100, int(bps_pct)))
    front_raw = max(0, min(65535, int(round(front_kph * 10.0))))
    rear_raw  = max(0, min(65535, int(round(rear_kph  * 10.0))))
    steer_s16 = max(-32768, min(32767, int(steering_deg)))

    data = struct.pack('<BBHHh',
                       apps_b, bps_b,
                       front_raw, rear_raw,
                       steer_s16)
    return can.Message(arbitration_id=ID_BENCH_OVERRIDE,
                       data=data, is_extended_id=False)


# =============================================================================
# Simulator
# =============================================================================
class DryBenchSim:
    """Owns the bus, the periodic TX tasks, and the RX listener thread."""

    def __init__(self, bus: can.BusABC, scenario: Scenario) -> None:
        self._bus = bus
        self._scen = scenario
        self._stop = threading.Event()
        self._tasks: list[can.CyclicSendTaskABC] = []
        self._rx_thread: Optional[threading.Thread] = None

        self._last_torque_dnm: Optional[int] = None
        self._last_torque_lim_dnm: Optional[int] = None
        self._last_print_wall_s: float = 0.0
        self._rx_msg_count: int = 0

    # ---------- TX (spoof) --------------------------------------------------
    def start_spoof(self) -> None:
        """Install the periodic transmit tasks."""
        # --- MCM feedback (hot group) ---------------------------------------
        mcm_frames = [
            build_frame_a5_motor_rpm(self._scen.motor_rpm),
            build_frame_a7_dc_bus(self._scen.dc_bus_volts),
            build_frame_a2_motor_temp(self._scen.motor_temp_c),
            build_frame_aa_status(self._scen.inverter_enabled,
                                  self._scen.lockout_cleared),
        ]
        for frame in mcm_frames:
            task = self._bus.send_periodic(frame, self._scen.period_mcm_hot_s)
            self._tasks.append(task)

        # --- Ghost wheel speeds --------------------------------------------
        wss_frames = [
            build_frame_wss_mmps(self._scen.front_wheel_kph,
                                 self._scen.rear_wheel_kph),
            build_frame_wss_rpm(self._scen.front_wheel_kph,
                                self._scen.rear_wheel_kph),
        ]
        for frame in wss_frames:
            task = self._bus.send_periodic(frame, self._scen.period_wss_s)
            self._tasks.append(task)

        # --- Virtual HIL sensor override (0x5FE) ---------------------------
        # This is the important one. If the VCU booted in bench mode
        # (IO_DI_06 pulled low), these values drive APPS, BPS, WSS, SAS
        # directly -- bypassing the ADC / PWD pins -- so traction control,
        # regen blending, DRS hysteresis, and the 80 kW ceiling can all
        # be exercised from a PC + CAN dongle alone.
        bench_frame = build_frame_bench_override(
            apps_pct    = self._scen.apps_percent,
            bps_pct     = self._scen.bps_percent,
            front_kph   = self._scen.front_wheel_kph,
            rear_kph    = self._scen.rear_wheel_kph,
            steering_deg= self._scen.steering_deg,
        )
        self._tasks.append(
            self._bus.send_periodic(bench_frame, self._scen.period_bench_s))

        logging.info("Spoof TX armed: %d periodic tasks running",
                     len(self._tasks))
        logging.info(
            "Bench override (0x%03X): APPS=%d%%  BPS=%d%%  "
            "WSS F/R=%.1f/%.1f kph  SAS=%d deg",
            ID_BENCH_OVERRIDE,
            self._scen.apps_percent, self._scen.bps_percent,
            self._scen.front_wheel_kph, self._scen.rear_wheel_kph,
            self._scen.steering_deg)

    # ---------- RX (listen for 0xC0) ---------------------------------------
    def start_listener(self) -> None:
        self._rx_thread = threading.Thread(
            target=self._rx_loop, name="mcm-cmd-listener", daemon=True)
        self._rx_thread.start()

    def _rx_loop(self) -> None:
        while not self._stop.is_set():
            try:
                # Short timeout so Ctrl+C is responsive even without traffic.
                msg = self._bus.recv(timeout=0.25)
            except can.CanError as exc:
                logging.warning("bus.recv CAN error: %s", exc)
                continue
            if msg is None:
                continue
            if msg.arbitration_id != ID_MCM_TORQUE_CMD:
                continue
            self._rx_msg_count += 1
            self._handle_mcm_torque_cmd(msg)

    def _handle_mcm_torque_cmd(self, msg: can.Message) -> None:
        """Decode byte 0 + byte 1 as sbyte2 LE in deci-Newton-meters."""
        if len(msg.data) < 2:
            logging.warning("0xC0 frame too short: %d bytes", len(msg.data))
            return

        torque_dnm = struct.unpack('<h', bytes(msg.data[0:2]))[0]
        torque_lim_dnm = None
        if len(msg.data) >= 8:
            torque_lim_dnm = struct.unpack('<h', bytes(msg.data[6:8]))[0]

        direction = msg.data[4] if len(msg.data) >= 5 else 0
        inv_en    = msg.data[5] if len(msg.data) >= 6 else 0

        # Throttle console output to ~20 Hz so real-time feel is preserved
        # without hosing the terminal on a 100 Hz bus.
        now = time.monotonic()
        if (now - self._last_print_wall_s) >= 0.05 or \
           torque_dnm != self._last_torque_dnm:
            self._last_print_wall_s = now
            self._last_torque_dnm = torque_dnm
            self._last_torque_lim_dnm = torque_lim_dnm

            torque_nm = torque_dnm / 10.0
            torque_lim_nm = (torque_lim_dnm / 10.0
                             if torque_lim_dnm is not None
                             else float('nan'))
            sys.stdout.write(
                "\r[0xC0] torque={:+7.1f} Nm  limit={:6.1f} Nm  "
                "dir={}  inv_en={}  rx#={:<6d}".format(
                    torque_nm, torque_lim_nm, direction, inv_en,
                    self._rx_msg_count))
            sys.stdout.flush()

    # ---------- lifecycle ---------------------------------------------------
    def stop(self) -> None:
        if self._stop.is_set():
            return
        self._stop.set()
        for task in self._tasks:
            try:
                task.stop()
            except Exception as exc:
                logging.warning("task.stop() raised: %s", exc)
        self._tasks.clear()
        if self._rx_thread is not None:
            self._rx_thread.join(timeout=1.0)
        sys.stdout.write("\n")
        sys.stdout.flush()


# =============================================================================
# Entry point
# =============================================================================
def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Phase-1 dry-bench CAN simulator for the SJSU FSAE VCU.")
    p.add_argument('--interface', default='virtual',
                   choices=['virtual', 'pcan', 'socketcan', 'slcan',
                            'kvaser', 'vector'],
                   help="python-can interface backend (default: virtual)")
    p.add_argument('--channel', default='dry_bench',
                   help="Channel / device identifier. "
                        "e.g. 'PCAN_USBBUS1', 'can0', 'COM3'.")
    p.add_argument('--bitrate', type=int, default=500_000,
                   help="CAN bitrate in bps (default 500000).")
    p.add_argument('--verbose', '-v', action='store_true',
                   help="Enable DEBUG logging.")
    return p.parse_args()


def main() -> int:
    args = _parse_args()
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format='%(asctime)s  %(levelname)s  %(message)s')

    logging.info("Opening bus: interface=%s channel=%s bitrate=%d",
                 args.interface, args.channel, args.bitrate)

    try:
        bus = can.Bus(interface=args.interface,
                      channel=args.channel,
                      bitrate=args.bitrate)
    except Exception as exc:
        logging.error("Failed to open CAN bus: %s", exc)
        return 2

    sim = DryBenchSim(bus, SCEN)

    # Graceful shutdown on Ctrl+C / SIGTERM.
    shutdown_evt = threading.Event()

    def _on_signal(signum, _frame):
        logging.info("Signal %d received -- shutting down...", signum)
        shutdown_evt.set()

    signal.signal(signal.SIGINT,  _on_signal)
    signal.signal(signal.SIGTERM, _on_signal)

    logging.info("Scenario:"
                 " wheels F/R = %.1f/%.1f kph,"
                 " RPM = %d,"
                 " Vdc = %.0f V,"
                 " Tmotor = %.0f C,"
                 " inv_en=%s lockout_cleared=%s",
                 SCEN.front_wheel_kph, SCEN.rear_wheel_kph,
                 SCEN.motor_rpm, SCEN.dc_bus_volts, SCEN.motor_temp_c,
                 SCEN.inverter_enabled, SCEN.lockout_cleared)
    logging.info("Listening for 0x%03X (MCM torque command)...",
                 ID_MCM_TORQUE_CMD)
    logging.info("Ctrl+C to stop.\n")

    try:
        sim.start_spoof()
        sim.start_listener()
        while not shutdown_evt.is_set():
            shutdown_evt.wait(timeout=0.5)
    finally:
        sim.stop()
        try:
            bus.shutdown()
        except Exception as exc:
            logging.warning("bus.shutdown() raised: %s", exc)

    logging.info("Clean exit.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
