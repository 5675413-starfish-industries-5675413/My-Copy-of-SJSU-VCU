"""
Unified power-limit tests — backend-agnostic, run via --backend sil|hil.

Usage:
    pytest sre_test/tests/test_power_limit.py --backend sil
    pytest sre_test/tests/test_power_limit.py --backend hil

All assertions use canonical response keys (result["power_limit"]["status"],
not the SIL-specific "pl_status"). Fields marked SIL-only or HIL-only in
sre_test/core/response.py are not tested here.
"""

from sre_test.core.config import DynamicConfig, configs_to_json


def test_pl_activates(env):
    """Power limit activates when motor is drawing power and plToggle is enabled."""
    mcm = DynamicConfig("MotorController")
    mcm.DC_Voltage = 1500
    mcm.DC_Current = 800
    mcm.set("motorRPM", 1800)

    tps = DynamicConfig("TorqueEncoder")
    tps.travelPercent = 0.5
    tps.calibrated = True

    env.send_inputs(mcm, tps)
    result = env.receive_outputs(timeout=2.0)

    pl = result.get("power_limit", {})
    assert pl.get("status") is True, (
        f"Expected power_limit.status=True, got: {pl}"
    )
    print(f"power_limit.status={pl.get('status')}, mode={pl.get('mode')}, "
          f"torque_command={pl.get('torque_command')}")


def test_pl_torque_command_is_float(env):
    """Power limit torque command is a float when PL is active."""
    mcm = DynamicConfig("MotorController")
    mcm.DC_Voltage = 1500
    mcm.DC_Current = 800
    mcm.set("motorRPM", 1800)

    tps = DynamicConfig("TorqueEncoder")
    tps.travelPercent = 0.5
    tps.calibrated = True

    env.send_inputs(mcm, tps)
    result = env.receive_outputs(timeout=2.0)

    pl = result.get("power_limit", {})
    torque = pl.get("torque_command")
    assert torque is not None, "torque_command missing from power_limit response"
    assert isinstance(torque, (int, float)), (
        f"Expected torque_command to be numeric, got {type(torque)}: {torque}"
    )
    print(f"torque_command={torque}")
