"""
Parameter Injector for SRE VCU testing

To save space on the VCU CAN bus, this module uses one CAN ID to manipulate subsystem/algorithm struct parameters.
Flow: Pass struct name and parameter name → look up (identifier, param_number) from struct_members_output.json → build CAN frame with integer IDs → VCU maps to struct field via its sorted parameter table.
"""

import struct
import json
from pathlib import Path
from typing import Optional
import can
from .can_interface import CANInterface

# CAN ID(s)
HIL_CAN_ID_INJECT = 0x5FE
HIL_CAN_ID_RESPONSE = 0     # to be changed
"""
raise NotImplementedError(
        "Param lookup requires SIL to emit identifier/param_number in struct_members_output.json. "
        "Implement this function once that JSON update is available."
    )
"""


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


def build_inject_message(identifier: int, param_number: int, value: int | float) -> can.Message:
    """
    Build parameter injection CAN message.
    """

    data = bytearray(8)

    # Byte 0: struct identifier
    data[0] = identifier & 0xFF

    # Byte 1: parameter number
    data[1] = param_number & 0xFF

    # Bytes 2-3: reserved
    data[2] = 0x00
    data[3] = 0x00

    # Bytes 4-7: Parameter value (little-endian)
    if isinstance(value, float):
        # Pack float as IEEE 754 32-bit (little-endian)
        float_bytes = struct.pack("<f", value)
        data[4:8] = float_bytes
    else:
        # Pack integer
        if value < 0:
            value = (1 << 32) + value
        value &= 0xFFFFFFFF

        data[4] = value & 0xFF
        data[5] = (value >> 8) & 0xFF
        data[6] = (value >> 16) & 0xFF
        data[7] = (value >> 24) & 0xFF

    return can.Message(
        arbitration_id=HIL_CAN_ID_INJECT,
        data=bytes(data),
        is_extended_id=False
    )

def parse_injection_response_message(msg: can.Message) -> dict:
    ""

### Injector Class ###
class HILParamInjector:
    """
    Interface for parameter injection.
    """

    def __init__(self, can_interface: "CANInterface", response_timeout: float = 0.1):
        self.can = can_interface
        self.timeout = response_timeout

    def inject(self, struct_name: str, param_name: str, value: int | float, wait_response: bool = False):
        """
        Inject parameter into VCU.
        """

        identifier, param_number = _lookup_param(struct_name, param_name)
        msg = build_inject_message(identifier, param_number, value)
        success = self.can.send(msg)

        if not success:
            return False

        if wait_response:
            return self._wait_response()
        return True

    def _wait_response(self) -> Optional[dict]:
        """Wait for and parse a response message"""
        import time
        deadline = time.time() + self.timeout

        while time.time() < deadline:
            msg = self.can.receive(timeout=0.01)

            if msg and msg.arbitration_id == HIL_CAN_ID_RESPONSE:
                return parse_injection_response_message(msg)
            
        return None
    
