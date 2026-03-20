import types

import pytest

from sre_test.hil.core.power_limit import PowerLimitDecoder, PowerLimitMonitor


class _FakeDbcMsg:
    def __init__(self, frame_id: int):
        self.frame_id = frame_id


class _FakeDBC:
    """Minimal DBC shim for unit-testing PowerLimitDecoder."""

    def __init__(self, id_a: int, id_b: int):
        self._id_a = id_a
        self._id_b = id_b
        self._decoded_by_id: dict[int, dict] = {}

    def get_message(self, name: str):
        if name == PowerLimitDecoder.MSG_STATUS_A:
            return _FakeDbcMsg(self._id_a)
        if name == PowerLimitDecoder.MSG_STATUS_B:
            return _FakeDbcMsg(self._id_b)
        raise KeyError(name)

    def set_decoded_signals(self, frame_id: int, signals: dict):
        self._decoded_by_id[frame_id] = {"signals": dict(signals)}

    def decode_message(self, msg):
        try:
            return self._decoded_by_id[msg.arbitration_id]
        except KeyError as exc:
            raise KeyError(f"No decoded signals registered for {msg.arbitration_id}") from exc


class _FakeCAN:
    def __init__(self, messages):
        self._messages = list(messages)

    def receive(self, timeout: float = 0.0):
        if not self._messages:
            return None
        return self._messages.pop(0)


def _msg(arbitration_id: int):
    """Create a minimal message-like object with just arbitration_id."""
    return types.SimpleNamespace(arbitration_id=arbitration_id, data=b"")


def test_decoder_ignores_none_message():
    dbc = _FakeDBC(id_a=0x511, id_b=0x512)
    dec = PowerLimitDecoder(dbc)

    status = dec.decode(None)
    assert status is dec.get_status()
    

def test_monitor_filters_non_pl_frames_then_decodes_pl():
    id_a, id_b = 0x511, 0x512
    dbc = _FakeDBC(id_a=id_a, id_b=id_b)
    dbc.set_decoded_signals(id_b, {"VCU_POWERLIMIT_getTorqueCommand_Nm": 12.0, "VCU_POWERLIMIT_PID_getIntegral": 0})

    can = _FakeCAN([_msg(0x123), _msg(id_b)])
    mon = PowerLimitMonitor(dbc, can)

    assert mon.recv(timeout=0.0) is None
    status = mon.recv(timeout=0.0)
    assert status is not None
    assert status.torque_command == pytest.approx(12.0)
