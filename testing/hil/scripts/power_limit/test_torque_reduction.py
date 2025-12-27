from sre_hil.core import CANInterface, DBCLoader
from sre_hil.core import MCM
import time

can = CANInterface()
dbc = DBCLoader()
mcm = MCM(dbc, can)

# Set parameters
mcm.set_rpm(4000)
mcm.set_voltage(350.0)
mcm.set_current(175.0)

# 350.0 * 175.0 = 61250 W = 61.25 kW -> compare to target power limit (40 kW in this case)
# Given 350V × 175A = 61.25 kW (exceeds 40 kW limit), what torque does the power limiter output?

TX_PERIOD_S = 0.01  # 100 Hz
TEST_DURATION_S = 5.0

# PL status to monitor for torque command
pl_status_b = dbc.get_message("VCU_Power_Limit_Status_BMsg")

# t_start = time.time()
t_end = time.time() + TEST_DURATION_S
next_tx = 0.0
# next_tx = t_start + 2.0       # Start injecting after 2 seconds

try:
    while time.time() < t_end:
        now = time.time()

        # Send MCM inputs periodically
        if now >= next_tx:
            mcm.send_all()
            next_tx = now + TX_PERIOD_S

        # Get VCU response for resulting torque command
        msg = can.receive(timeout=0.001)
        if msg and msg.arbitration_id == pl_status_b.frame_id:
            decoded = dbc.decode_message(msg)
            torque = decoded['signals'].get("VCU_POWERLIMIT_getTorqueCommand_Nm")
            print(f"Torque command: {torque} Nm")

finally:
    can.shutdown()