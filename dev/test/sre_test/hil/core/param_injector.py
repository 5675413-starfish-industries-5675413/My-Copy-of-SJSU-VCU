"""
Parameter Injector for SRE VCU testing

To save space on the VCU CAN bus, this module uses one CAN ID to manipulate subsystem/algorithm struct parameters.
Flow: Pass parameter name as string → encode string to 32-bit value → inject value into CAN → VCU decodes value then maps to struct parameter.
"""

import struct
from typing import Optional
import can

from .can_interface import CANInterface

# Encoding constants
CHARSET = " abcdefghijklmnopqrstuvwxyz01234"
BITS_PER_CHAR = 5
MAX_NAME_LENGTH = 6
CHAR_MASK = 0x1F

# CAN ID(s)
HIL_CAN_ID_INJECT = 0x5FE
HIL_CAN_ID_RESPONSE = 0     # to be changed

def encode_name(name: str) -> int:
    """
    Encode parameter name (up to 6 characters) into a 32-bit integer.
    """

    if len(name) > MAX_NAME_LENGTH:
        raise ValueError(f"Name '{name}' exceeds {MAX_NAME_LENGTH} characters.")
    if len(name) == 0:
        raise ValueError("Name cannot be empty.")
    
    name_lower = name.lower()

    encoded = 0
    for char in name_lower:
        if char not in CHARSET:
            raise ValueError(f"Invalid character: '{char}' (allowed: a-z, 0-4, space)")
        idx = CHARSET.index(char)
        encoded = (encoded << BITS_PER_CHAR) | idx

    # Fill remaining positions with zeros (terminator)
    remaining =  MAX_NAME_LENGTH - len(name_lower)
    encoded <<= (remaining * BITS_PER_CHAR)

    return encoded

def decode_name(encoded: int) -> str:
    """
    Decode 32-bit integer back into parameter name string.
    """

    chars = []
    for i in range(MAX_NAME_LENGTH):
        shift = (MAX_NAME_LENGTH - 1 - i) * BITS_PER_CHAR
        idx = (encoded >> shift) & CHAR_MASK
        if idx == 0:    # Terminator
            break
        chars.append(CHARSET[idx])
    
    return ''.join(chars)

def build_inject_message(name: str, value: int | float) -> can.Message:
    """
    Build parameter injection CAN message.
    """

    encoded_name = encode_name(name)

    data = bytearray(8)

    # Bytes 0-3: Encoded parameter name (big-endian)
    data[0] = (encoded_name >> 24) & 0xFF
    data[1] = (encoded_name >> 16) & 0xFF
    data[2] = (encoded_name >> 8) & 0xFF
    data[3] = encoded_name & 0xFF

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

    def inject(self, name: str, value: int | float, wait_response: bool = False):
        """
        Inject parameter into VCU.
        """

        msg = build_inject_message(name, value)
        success = self.can.send(msg)

        if not success:
            return False
        
        if wait_response:
            return self._wait_response(name)
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
    
### VERIFICATION/TESTING ###
def verify_encoding():
    """Verify encoding/decoding scheme works as intended"""

    test_names = ["pltog", "plstat", "plmode", "lctog", "rgntog", "a", "zzzzzz"]

    all_passed = True
    for name in test_names:
        try:
            encoded = encode_name(name)
            decoded = decode_name(encoded)
            passed = (decoded == name)
            all_passed &= passed

            status = "PASS" if passed else "FAIL"
            print(f"  '{name}' -> 0x{encoded:08X} -> '{decoded}'  [{status}]")
        except ValueError as e:
            print(f"  '{name}' -> ERROR: {e}")
            all_passed = False
    
    print("-" * 50)
    print(f"Overall: {'ALL PASSED' if all_passed else 'SOME FAILED'}")
    return all_passed

if __name__ == "__main__":
    verify_encoding()