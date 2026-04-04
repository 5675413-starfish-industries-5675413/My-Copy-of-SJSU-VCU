"""
Efficiency-related tests.
"""

from types import DynamicClassAttribute
import pytest
from sre_test.sil.core.compiler import SILCompiler
from sre_test.sil.core.simulator import SILSimulator
from sre_test.sil.core.helpers.config import DynamicConfig, configs_to_json
from sre_test.sil.core.helpers.state import reset_struct_members_to_null
from sre_test.sil.core.helpers.path import (
    DEV,
    INC,
    CONFIG,
    DATA,
    STRUCT_MEMBERS,
)

sim = None


@pytest.fixture(scope="module", autouse=True)
def setup_simulator():
    """Automatically create simulator for all tests - no parameter needed."""
    global sim
    reset_struct_members_to_null()
    sim = SILSimulator.create(auto_compile=True)
    yield
    sim.stop()
    sim = None

def test_path():
    """Checking if paths are correct."""
    # Your test logic here
    print(DEV)
    print(INC)
    print(CONFIG)
    print(DATA)
    print(STRUCT_MEMBERS)
    assert True


def test_DynamicConfig():
    """Checking if DynamicConfig works."""
    config = DynamicConfig("Efficiency")
    config.energyBudget_kWh = 0.3
    config.efficiencyToggle = True
    config2 = DynamicConfig("PowerLimit")
    config2.plToggle = 100
    configs_to_json(config, config2)
    assert config.energyBudget_kWh == 0.3


def test_point(): 
    """Single point simulation test"""
    # Create MotorController config with motorRPM
    mcm_config = DynamicConfig("MotorController")
    mcm_config.DC_Voltage = 1500
    mcm_config.DC_Current = 800
    # mcm_config.motorRPM = 1800
    mcm_config.set("motorRPM", 1800)
    
    tps_config = DynamicConfig("TorqueEncoder")
    tps_config.travelPercent = 0.5
    tps_config.calibrated = True

    # Update struct_members_output.json with the config
    configs_to_json(mcm_config, tps_config)
    
    sim.send_structs("MotorController", "TorqueEncoder")

    # Let the control loop update plStatus from inputs (do not force it in config).
    pl_status = None
    motor_rpm = None
    
    response = sim.receive()
    pl = response.get("PowerLimit")
    pl_torque_command = pl.get("plTorqueCommand")
    print(f"received pl_torque_command: {pl_torque_command}")
    
    # for _ in range(5):
    #     response = sim.receive(timeout=2.0)
    #     power_limit = {}
    #     mcm = {}
    #     if response:
    #         power_limit = response.get("PowerLimit") or response.get("power_limit", {})
    #         mcm = (
    #             response.get("MotorController")
    #             or response.get("motor_controller")
    #             or response.get("mcm", {})
    #         )
    #     pl_status = power_limit.get("plStatus") if power_limit else None
    #     if pl_status is None and power_limit:
    #         pl_status = power_limit.get("pl_status")
    #     motor_rpm = mcm.get("motorRPM")
    #     if motor_rpm is None and mcm:
    #         motor_rpm = mcm.get("motor_rpm")
        # if pl_status is True:
        #     break
    # sim.send_structs("MotorController", "TorqueEncoder")
    
    

    # assert pl_status is True
    # print(f"received pl_status: {pl_status}")
    # print(f"received motor_rpm: {motor_rpm}")
    
def test_regen():
    regen_config = DynamicConfig("Regen")
    regen_config.padMu = 0.4
    regen_config.regenToggle = True
    regen_config.bpsTorqueNm = 0
    
    brake_config = DynamicConfig("BrakePressureSensor")
    brake_config.bps1_voltage = 2225
    brake_config.bps0_voltage = 1208
    
    
    mcm_config = DynamicConfig("MotorController")
    mcm_config.motorRPM = 2000
    
    configs_to_json(regen_config, brake_config, mcm_config)
    
    sim.send_structs("MotorController", "BrakePressureSensor", "Regen")
    
    response = sim.receive()
    regenResponse = response.get("Regen", {}) if response else {}
    
    mcmResponse = response.get("MotorController", {}) if response else {}
    
    print(f"Regen toggle is: {regenResponse.get('regenToggle')}")
    # assert regenResponse.get("bpsTorqueNm") == -102.94
    print(f"Brake pressure difference is: {regenResponse.get('bpsTorqueNm')}")
    # print(f"Pad mu is: {regenResponse.get('padMu')}")
    print(f"Regen torque command is: {mcmResponse.get('regenTorqueCommand')}")
    
def test_launch():
    wws_config = DynamicConfig("WheelSpeeds")
    wws_config.frequency_FL = 100
    wws_config.frequency_FR = 100
    wws_config.frequency_RL = 200
    wws_config.frequency_RR = 200
    
    lc_config = DynamicConfig("LaunchControl")
    lc_config.lcToggle = True
    lc_config.state = 2
    lc_config.phase = 1
    lc_config.mode = 0
    
    
    tps_config = DynamicConfig("TorqueEncoder")
    tps_config.tps0_percent = 0.95
    
    bps_config = DynamicConfig("BrakePressureSensor")
    bps_config.percent = 0.00
    
    configs_to_json(wws_config, lc_config, tps_config, bps_config)
    
    sim.send_structs("WheelSpeeds", "LaunchControl", "TorqueEncoder", "BrakePressureSensor")
    
    response = sim.receive()
    LCResponse = response.get("LaunchControl", {}) if response else {}
    print(f"PID output is: {LCResponse.get('pidOutput')}")
    
def test_efficiency2():
    mcm_config = DynamicConfig("MotorController")

    mcm_config.DC_Voltage = 200
    mcm_config.DC_Current = 300
    
    eff_config = DynamicConfig("Efficiency")
    eff_config.efficiencyToggle = True
    
    configs_to_json(eff_config, mcm_config)
    
    sim.send_structs("Efficiency", "MotorController")
    
    response = sim.receive()
    effResponse = response.get("Efficiency", {}) if response else {}
    print(f"StraightTime is: {effResponse.get('straightTime_s')}")
    print(f"CornerTime is: {effResponse.get('cornerTime_s')}")
    print(f"CornerEnergy is: {effResponse.get('cornerEnergy_kWh')}")
    print(f"StraightEnergy is: {effResponse.get('straightEnergy_kWh')}")
    print(f"LapEnergy is: {effResponse.get('lapEnergy_kWh')}")
    print(f"LapDistance is: {effResponse.get('lapDistance_km')}")
    print(f"FinishedLap is: {effResponse.get('finishedLap')}")

def test_encoder():
    tps_config = DynamicConfig("TorqueEncoder")
    tps_config.tps0_percent = 0.3
    tps_config.tps1_percent = 0.4
    
    configs_to_json(tps_config)
    
    sim.send_structs("TorqueEncoder")
    
    response = sim.receive()
    
    tpsResponse = response.get("TorqueEncoder", {}) if response else {}
    print(f"TPS0 percent is: {tpsResponse.get('tps0_percent')}")
    print(f"TPS1 percent is: {tpsResponse.get('tps1_percent')}")
    print(f"Travel percent is: {tpsResponse.get('travelPercent')}")
