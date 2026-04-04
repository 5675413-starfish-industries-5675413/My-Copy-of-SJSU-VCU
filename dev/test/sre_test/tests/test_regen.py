"""
Efficiency Algorithm Test Script

Usage:
    pytest sre_test/tests/test_efficiency.py --backend sil
    pytest sre_test/tests/test_efficiency.py --backend hil

    TBD, will turn to:
    sre test efficiency --backend sil
    sre test efficiency --backend hil
"""

from sre_test.core.config import DynamicConfig
import time

def test_regen(env):
    regen = DynamicConfig("Regen")
    regen.padMu = 0.4
    regen.regenToggle = True
    regen.bpsTorqueNm = 0
    
    brake = DynamicConfig("BrakePressureSensor")
    brake.bps1_voltage = 2225
    brake.bps0_voltage = 1208
    
    mcm = DynamicConfig("MotorController")
    mcm.motorRPM = 2000
    
    env.send_inputs(regen, brake, mcm)
    result = env.receive_outputs(timeout=2.0)
    
    regen_status = result.get("Regen")
    mcm_status = result.get("MotorController")
    print(f"Regen toggle is: {regen_status.get('regenToggle')}")
    # assert regenResponse.get("bpsTorqueNm") == -102.94
    print(f"Brake pressure difference is: {regen_status.get('bpsTorqueNm')}")
    # print(f"Pad mu is: {regen_status.get('padMu')}")
    print(f"Regen torque command is: {mcm_status.get('regenTorqueCommand')}")
    