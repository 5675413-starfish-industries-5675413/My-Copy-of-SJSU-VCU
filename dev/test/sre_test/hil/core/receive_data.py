import json
import time
from pathlib import Path
from typing import Optional

from .can_interface import CANInterface
from .hil_state_machine import HILState, HILStateMachine

HIL_CAN_ID_RESPONSE = 0x514

HIL_MUX_DIAG = 0x01
HIL_MUX_ACK = 0x02
HIL_MUX_VALUE = 0x03

_DIAG_END_IDENTIFIER = 0xFF

STRUCT_MEMBERS_JSON = (
    Path(__file__).resolve().parents[4]
    / "dev"
    / "test"
    / "sre_test"
    / "sil"
    / "config"
    / "struct_members_output.json"
)


class HILReceiveHandler:
    def __init__(self, can_interface: CANInterface, state_machine: HILStateMachine):
        self.can = can_interface
        self.state_machine = state_machine
        self._diag_counts: dict[int, int] = {}
        self._expected_counts: Optional[dict[int, int]] = None

    def listen(self, timeout: float) -> Optional[int]:
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.state_machine.is_timed_out(timeout):
                self.state_machine.transition(HILState.ERROR)
                return None

            msg = self.can.receive(timeout=0.01)
            if msg is None or msg.arbitration_id != HIL_CAN_ID_RESPONSE:
                continue

            raw = bytes(msg.data).ljust(8, b"\x00")[:8]
            mux = raw[0]

            if mux == HIL_MUX_DIAG:
                self._on_diag(raw)
                continue
            if mux == HIL_MUX_ACK:
                self._on_ack(raw)
                continue
            if mux == HIL_MUX_VALUE:
                return self._on_value(raw)

        return None

    def _on_diag(self, raw: bytes) -> None:
        identifier = raw[1]
        count = raw[2]
        if self.state_machine.state == HILState.INIT_PENDING:
            self.state_machine.transition(HILState.DIAGNOSTIC)
        if self.state_machine.state != HILState.DIAGNOSTIC:
            return

        self._diag_counts[identifier] = count
        expected = self._get_expected_counts()
        if expected and len(self._diag_counts) >= len(expected):
            if self._diag_counts == expected:
                self.state_machine.transition(HILState.READY)
            else:
                self.state_machine.transition(HILState.ERROR)

    def _on_ack(self, raw: bytes) -> None:
        if self.state_machine.state != HILState.INJECT_PENDING:
            return
        status = raw[3]
        if status == 0x00:
            self.state_machine.transition(HILState.READY)
        else:
            self.state_machine.transition(HILState.ERROR)

    def _on_value(self, raw: bytes) -> Optional[int]:
        if self.state_machine.state != HILState.REQUEST_PENDING:
            return None
        value = int.from_bytes(raw[4:8], "little", signed=True)
        self.state_machine.transition(HILState.READY)
        return value

    def _get_expected_counts(self) -> Optional[dict[int, int]]:
        if self._expected_counts is not None:
            return self._expected_counts
        try:
            with STRUCT_MEMBERS_JSON.open("r", encoding="utf-8") as f:
                data = json.load(f)
        except (OSError, ValueError):
            self._expected_counts = {}
            return self._expected_counts

        expected: dict[int, int] = {}
        for entry in data:
            identifier = entry.get("identifier", entry.get("id"))
            if identifier is None:
                continue
            params = entry.get("parameters", {})
            expected[int(identifier) & 0xFF] = len(params)
        self._expected_counts = expected
        return self._expected_counts
