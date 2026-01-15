import os
import time

import can
import pytest

from sre_test.hil.core import CANInterface, DBCLoader, PowerLimitMonitor


def _only_known_signals(msg, signals: dict) -> dict:
    names = {s.name for s in msg.signals}
    return {k: v for k, v in signals.items() if k in names}


def _fill_required_signals(msg, overrides: dict) -> dict:
    """cantools requires all signals for encoding."""
    filled = {s.name: 0 for s in msg.signals}
    filled.update(_only_known_signals(msg, overrides))
    return filled


def test_power_limit_monitor_decodes_on_virtual_bus():
    """Decodes DBC-encoded PL frames on a virtual CAN bus."""

    channel = "sre_hil_pl_test"

    old_env = {
        "SRE_HIL_CAN_INTERFACE": os.environ.get("SRE_HIL_CAN_INTERFACE"),
        "SRE_HIL_CAN_CHANNEL": os.environ.get("SRE_HIL_CAN_CHANNEL"),
        "SRE_HIL_CAN_BITRATE": os.environ.get("SRE_HIL_CAN_BITRATE"),
    }
    os.environ["SRE_HIL_CAN_INTERFACE"] = "virtual"
    os.environ["SRE_HIL_CAN_CHANNEL"] = channel
    os.environ["SRE_HIL_CAN_BITRATE"] = "500000"

    dbc = DBCLoader()

    msg_a = dbc.get_message("VCU_Power_Limit_Status_AMsg")
    msg_b = dbc.get_message("VCU_Power_Limit_Status_BMsg")

    sender = can.interface.Bus(interface="virtual", channel=channel)

    can_if = CANInterface()
    pl = PowerLimitMonitor(dbc, can_if)

    try:
        data_a = msg_a.encode(
            _fill_required_signals(
                msg_a,
                {
                    "VCU_POWERLIMIT_getInitialisationThreshold_W": 123,
                    "VCU_POWERLIMIT_getMode_W": 2,
                    "VCU_POWERLIMIT_getStatus_W": 1,
                    "VCU_POWERLIMIT_PID_getTotalError": 10,
                    "VCU_POWERLIMIT_PID_getProportional": -3,
                    "VCU_PID_getTotalError": 10,
                    "VCU_PID_getProportional": -3,
                },
            )
        )
        sender.send(can.Message(arbitration_id=msg_a.frame_id, data=data_a, is_extended_id=False))

        torque_nm = 42.0
        data_b = msg_b.encode(
            _fill_required_signals(
                msg_b,
                {
                    "VCU_POWERLIMIT_PID_getIntegral": 7,
                    "VCU_POWERLIMIT_getTorqueCommand_Nm": torque_nm,
                    "VCU_POWERLIMIT_getTargetPower_W": 40,
                    "VCU_POWERLIMIT_PID_getOutput": -99,
                    "VCU_POWERLIMIT_getClampingMethod_W": 1,
                    "VCU_PID_getIntegral": 7,
                    "VCU_PID_getOutput": -99,
                },
            )
        )
        sender.send(can.Message(arbitration_id=msg_b.frame_id, data=data_b, is_extended_id=False))

        t_end = time.time() + 1.0
        got_b = None
        while time.time() < t_end:
            status = pl.recv(timeout=0.05)
            if status is None:
                continue
            if abs(status.torque_command - torque_nm) < 0.51:
                got_b = status
                break

        assert got_b is not None, "Did not receive/parse PL Status B frame on virtual bus"
        assert got_b.torque_command == pytest.approx(torque_nm)
        if "VCU_POWERLIMIT_PID_getIntegral" in {s.name for s in msg_b.signals}:
            assert got_b.pid_integral == 7
        if "VCU_POWERLIMIT_PID_getOutput" in {s.name for s in msg_b.signals}:
            assert got_b.pid_output == -99
        assert got_b.clamping_method == 1

    finally:
        try:
            sender.shutdown()
        except Exception:
            pass
        can_if.shutdown()
        for k, v in old_env.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
