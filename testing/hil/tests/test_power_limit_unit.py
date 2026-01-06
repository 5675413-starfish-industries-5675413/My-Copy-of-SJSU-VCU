import types

import pytest

from sre_hil.core.power_limit import PowerLimitDecoder, PowerLimitMonitor


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


def test_decoder_populates_status_a_fields():
    id_a, id_b = 0x511, 0x512
    dbc = _FakeDBC(id_a=id_a, id_b=id_b)
    dbc.set_decoded_signals(
        id_a,
        {
            "VCU_POWERLIMIT_getInitialisationThreshold_W": 123,
            "VCU_POWERLIMIT_getMode_W": 2,
            "VCU_POWERLIMIT_getStatus_W": 1,
            "VCU_POWERLIMIT_PID_getTotalError": 10,
            "VCU_POWERLIMIT_PID_getProportional": -3,
        },
    )

    dec = PowerLimitDecoder(dbc)
    status = dec.decode(_msg(id_a))

    assert status.initialization_threshold == 123
    assert status.mode == 2
    assert status.status is True
    assert status.pid_total_error == 10
    assert status.pid_proportional == -3


def test_decoder_populates_status_b_fields():
    id_a, id_b = 0x511, 0x512
    dbc = _FakeDBC(id_a=id_a, id_b=id_b)
    dbc.set_decoded_signals(
        id_b,
        {
            "VCU_POWERLIMIT_PID_getIntegral": 7,
            "VCU_POWERLIMIT_getTorqueCommand_Nm": 42.5,
            "VCU_POWERLIMIT_getTargetPower_W": 40000,
            "VCU_POWERLIMIT_PID_getOutput": -99,
            "VCU_POWERLIMIT_getClampingMethod_W": 1,
        },
    )

    dec = PowerLimitDecoder(dbc)
    status = dec.decode(_msg(id_b))

    assert status.pid_integral == 7
    assert status.torque_command == pytest.approx(42.5)
    assert status.target_power == 40000
    assert status.pid_output == -99
    assert status.clamping_method == 1


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
