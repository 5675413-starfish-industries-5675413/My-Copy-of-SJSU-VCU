"""
Parameter Reader for SRE VCU testing.
To save space on the CAN bus, one CAN ID is used for parameter reads. Flow:
struct name + parameter name → (identifier, param_number) from struct_members_output.json
→ build read request with integer IDs → VCU returns value in the same 8-byte layout as injection.
"""
import time
from typing import Optional
import can
from .can_interface import CANInterface
HIL_CAN_ID_READ = 0x5FE
HIL_CAN_ID_READ_RESPONSE = 0x514
def _lookup_param(struct_name: str, param_name: str) -> tuple[int, int]:
    """
    Look up (identifier, param_number) for a parameter from struct_members_output.json.
    NOTE: Stub. Implement once SIL emits identifier and param_number in that JSON:
      1. Load the JSON file.
      2. Find the entry matching struct_name.
      3. Return (entry["identifier"], entry["parameters"][param_name]["param_number"]).
    """
    raise NotImplementedError(
        "Param lookup requires SIL to emit identifier/param_number in struct_members_output.json. "
        "Implement this function once that JSON update is available."
    )
def build_read_request_message(identifier: int, param_number: int) -> can.Message:
    """8-byte read request: identifier + param index; value bytes zero."""
    data = bytearray(8)
    data[0] = identifier & 0xFF
    data[1] = param_number & 0xFF
    data[2] = 0x00
    data[3] = 0x00
    # bytes 4–7: zeros (read request)
    return can.Message(
        arbitration_id=HIL_CAN_ID_READ,
        data=bytes(data),
        is_extended_id=False,
    )
def parse_read_response_message(msg: can.Message) -> dict:
    """Parse read response payload (same 8-byte layout as injection)."""
    raw = bytes(msg.data).ljust(8, b"\x00")[:8]
    value = int.from_bytes(raw[4:8], "little", signed=True)
    return {
        "identifier": raw[0],
        "param_number": raw[1],
        "value": value,
    }
class HILParamReader:
    """Interface for parameter reads (by identifier / param_number on the wire)."""
    def __init__(self, can_interface: CANInterface, response_timeout: float = 0.2):
        self.can = can_interface
        self.timeout = response_timeout
    def send_read_request(self, identifier: int, param_number: int) -> bool:
        """Send a read request frame only (does not wait for a response)."""
        msg = build_read_request_message(identifier, param_number)
        return self.can.send(msg)
    def receive_read_response(
        self, identifier: int, param_number: int
    ) -> Optional[dict]:
        """Wait for a read response matching this identifier and param number."""
        want_id = identifier & 0xFF
        want_pn = param_number & 0xFF
        deadline = time.time() + self.timeout
        while time.time() < deadline:
            msg = self.can.receive(timeout=0.01)
            if msg is None or msg.arbitration_id != HIL_CAN_ID_READ_RESPONSE:
                continue
            raw = bytes(msg.data).ljust(8, b"\x00")[:8]
            if raw[0] != want_id or raw[1] != want_pn:
                continue
            return parse_read_response_message(msg)
        return None
    def read(self, identifier: int, param_number: int) -> Optional[dict]:
        """Send read request, then wait for the matching response."""
        if not self.send_read_request(identifier, param_number):
            return None
        return self.receive_read_response(identifier, param_number)
    def read_by_name(self, struct_name: str, param_name: str) -> Optional[dict]:
        """Resolve struct/param via struct_members_output.json, then read."""
        identifier, param_number = _lookup_param(struct_name, param_name)
        return self.read(identifier, param_number)
