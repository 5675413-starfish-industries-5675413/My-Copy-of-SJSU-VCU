import time
import os

import pytest

from sre_test.hil.core import CANInterface, DBCLoader, MCM
from sre_test.hil.core import PowerLimitMonitor

def _run(duration_s: float) -> bool:
    can = CANInterface()
    dbc = DBCLoader()
    mcm = MCM(dbc, can)
    pl = PowerLimitMonitor(dbc, can)

    # Set parameters
    mcm.set_rpm(4000)
    mcm.set_voltage(350.0)
    mcm.set_current(175.0)

    TX_PERIOD_S = 0.01  # 100 Hz

    t_end = time.time() + duration_s
    next_tx = 0.0
    received_any = False

    try:
        while time.time() < t_end:
            now = time.time()

            # Send MCM inputs periodically
            if now >= next_tx:
                mcm.send_all()
                next_tx = now + TX_PERIOD_S

            status = pl.recv(timeout=0.001)
            if status is not None:
                print(f"Torque command: {status.torque_command} Nm")
                received_any = True

        return received_any
    finally:
        can.shutdown()


def test_torque_reduction_integration():
    """Integration smoke test (requires configured CAN bus + running VCU)."""
    try:
        received_any = _run(duration_s=5.0)
    except Exception as exc:
        pytest.skip(f"CAN not available/configured: {exc}")
    if not received_any:
        require_vcu = os.environ.get("SRE_HIL_REQUIRE_VCU", "").strip().lower()
        if require_vcu in {"1", "true", "yes"}:
            assert received_any, "No VCU_Power_Limit_Status_BMsg received during test window"
        pytest.skip("No VCU_Power_Limit_Status_BMsg received (VCU not running / not on bus). Set SRE_HIL_REQUIRE_VCU=1 to make this a hard failure.")


if __name__ == "__main__":
    _run(duration_s=5.0)

