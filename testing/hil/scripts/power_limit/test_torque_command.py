import time
from pathlib import Path

import can
import cantools
import pytest

TX_PERIOD_S = 0.01  # 100 Hz

# Values to inject
MOTOR_RPM = 4000
DC_VOLTAGE = 350.0
DC_CURRENT = 175.0
TEST_DURATION_SECONDS = 5.0

DBC_PATH = Path(__file__).resolve().parents[2] / "dbcs" / "10.03.25_LC_Main.dbc"
dbc = cantools.database.load_file(str(DBC_PATH), strict=False)

mcm_rpm = dbc.get_message_by_name("MCM_Motor_Position_Info")
mcm_current = dbc.get_message_by_name("MCM_Current_Info")
mcm_voltage = dbc.get_message_by_name("MCM_Voltage_Info")
pl_status_b = dbc.get_message_by_name("VCU_Power_Limit_Status_BMsg")

def _run(duration_s: float) -> bool:
    bus = can.interface.Bus()

    print(f"Injecting RPM={MOTOR_RPM}, Voltage={DC_VOLTAGE}, Current={DC_CURRENT}")
    print("Waiting for torque command on 0x512...")

    t_end = time.time() + duration_s
    next_tx = 0.0
    received_any = False

    try:
        while time.time() < t_end:
            now = time.time()
            if now >= next_tx:
                data_pos = mcm_rpm.encode({"MCM_Motor_Speed": int(MOTOR_RPM)})
                data_vol = mcm_voltage.encode({"MCM_DC_Bus_Voltage": float(DC_VOLTAGE)})
                data_cur = mcm_current.encode({"MCM_DC_Bus_Current": float(DC_CURRENT)})

                bus.send(can.Message(arbitration_id=mcm_rpm.frame_id, data=data_pos, is_extended_id=False))
                bus.send(can.Message(arbitration_id=mcm_voltage.frame_id, data=data_vol, is_extended_id=False))
                bus.send(can.Message(arbitration_id=mcm_current.frame_id, data=data_cur, is_extended_id=False))

                next_tx = now + TX_PERIOD_S

            msg = bus.recv(0.001)
            if msg and msg.arbitration_id == pl_status_b.frame_id:
                decoded = pl_status_b.decode(bytes(msg.data))
                torque_nm = decoded.get("VCU_POWERLIMIT_getTorqueCommand_Nm")
                print(f"[0x512] Torque command: {torque_nm:.1f} Nm")
                received_any = True

        print("Done.")
        return received_any
    finally:
        bus.shutdown()


def test_torque_command_integration():
    """Integration smoke test (requires configured CAN bus + running VCU)."""
    try:
        received_any = _run(TEST_DURATION_SECONDS)
    except Exception as exc:
        pytest.skip(f"CAN not available/configured: {exc}")
    assert received_any, "No VCU_Power_Limit_Status_BMsg received during test window"


if __name__ == "__main__":
    _run(TEST_DURATION_SECONDS)
