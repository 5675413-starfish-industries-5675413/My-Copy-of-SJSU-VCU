"""
HIL → VCU send handler for SRE VCU testing.

Owns all frame construction and send logic for the 0x5FE channel.
Each command is multiplexed via byte 0:
  0x01 = HIL_CMD_INIT
  0x02 = HIL_CMD_INJECT
  0x03 = HIL_CMD_REQUEST

State-transition strategy: transition BEFORE sending to claim the pending
state atomically, then roll back on send failure.  This eliminates any
window where the bus has a frame in flight but the state machine still
says READY.
"""

import struct
import json
from pathlib import Path

import can

from .can_interface import CANInterface
from .hil_state_machine import HILStateMachine, HILState

HIL_CAN_ID = 0x5FE

STRUCT_MEMBERS_JSON = (
    Path(__file__).resolve().parents[4]
    / "dev"
    / "test"
    / "sre_test"
    / "sil"
    / "config"
    / "struct_members_output.json"
)


def _lookup_param(struct_name: str, param_name: str) -> tuple[int, int]:
    """
    Look up (identifier, param_number) for a parameter from struct_members_output.json.

    NOTE: This is a stub. It will be fully implemented once SIL generates
    identifier and param_number fields in struct_members_output.json.
    At that point, this function should:
      1. Load the JSON file.
      2. Find the entry matching struct_name.
      3. Return (entry["identifier"], entry["parameters"][param_name]["param_number"]).
    """
    with STRUCT_MEMBERS_JSON.open("r", encoding="utf-8") as f:
        data = json.load(f)

    entry = next((e for e in data if e.get("name") == struct_name), None)
    if entry is None:
        raise KeyError(f"Struct '{struct_name}' not found in {STRUCT_MEMBERS_JSON}")

    if param_name not in entry.get("parameters", {}):
        raise KeyError(f"Parameter '{param_name}' not found under struct '{struct_name}'")

    return (entry["identifier"], entry["parameters"][param_name]["param_number"])


class HILSendHandler:
    """Owns all frame construction and send logic for the 0x5FE channel."""

    def __init__(self, can_interface: CANInterface, state_machine: HILStateMachine):
        self.can = can_interface
        self.sm = state_machine

    def _send_frame(self, data: bytearray, target_state: HILState, rollback_state: HILState) -> bool:
        """ 
        Transition → target_state, send the frame, roll back on failure.

        The state machine's own transition table enforces which source states
        are legal, so callers don't need a separate guard.
        """
        self.sm.transition(target_state)

        msg = can.Message(
            arbitration_id=HIL_CAN_ID,
            data=bytes(data),
            is_extended_id=False,
        )

        if not self.can.send(msg):
            self.sm.transition(rollback_state)
            return False
        return True

    # ── send_init ────────────────────────────────────────────────────────
    def send_init(self) -> bool:
        """
        Build HIL_CMD_INIT frame and send on 0x5FE.
        Byte 0 = 0x01 (mux), bytes 1-7 = 0x00.
        Transitions: DISCONNECTED → INIT_PENDING (rolls back on send failure).
        """
        data = bytearray(8)
        data[0] = 0x01  # HIL_CMD_INIT
        return self._send_frame(data, HILState.INIT_PENDING, HILState.DISCONNECTED)

    # ── send_inject ──────────────────────────────────────────────────────
    def send_inject(self, identifier: int, param_number: int, value: int | float) -> bool:
        """
        Build HIL_CMD_INJECT frame and send on 0x5FE.
        Layout:
          byte 0 = 0x02 (mux)
          byte 1 = identifier
          byte 2 = param_number
          byte 3 = 0x00 (reserved)
          bytes 4-7 = value (little-endian)
        Transitions: READY → INJECT_PENDING (rolls back on send failure).
        """
        data = bytearray(8)
        data[0] = 0x02  # HIL_CMD_INJECT
        data[1] = identifier & 0xFF
        data[2] = param_number & 0xFF
        data[3] = 0x00

        if isinstance(value, float):
            data[4:8] = struct.pack("<f", value)
        else:
            if value < 0:
                value = (1 << 32) + value
            value &= 0xFFFFFFFF
            data[4] = value & 0xFF
            data[5] = (value >> 8) & 0xFF
            data[6] = (value >> 16) & 0xFF
            data[7] = (value >> 24) & 0xFF

        return self._send_frame(data, HILState.INJECT_PENDING, HILState.READY)

    # ── send_request ─────────────────────────────────────────────────────
    def send_request(self, identifier: int, param_number: int) -> bool:
        """
        Build HIL_CMD_REQUEST frame and send on 0x5FE.
        Layout:
          byte 0 = 0x03 (mux)
          byte 1 = identifier
          byte 2 = param_number
          bytes 3-7 = 0x00
        Transitions: READY → REQUEST_PENDING (rolls back on send failure).
        """
        data = bytearray(8)
        data[0] = 0x03  # HIL_CMD_REQUEST
        data[1] = identifier & 0xFF
        data[2] = param_number & 0xFF

        return self._send_frame(data, HILState.REQUEST_PENDING, HILState.READY)
