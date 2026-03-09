"""
Efficiency-related tests.
"""

import pytest
from sre_test.sil.core.compiler import SILCompiler
from sre_test.sil.core.simulator import SILSimulator
from sre_test.sil.core.helpers.config import DynamicConfig, configs_to_json
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
    sim = SILSimulator.create(auto_compile=False)
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
    # pl_status = None
    # for _ in range(5):
    response = sim.receive(timeout=2.0)
    power_limit = {}
    if response:
        power_limit = response.get("PowerLimit") or response.get("power_limit", {})
    pl_status = power_limit.get("plStatus") if power_limit else None
    if pl_status is None and power_limit:
        pl_status = power_limit.get("pl_status")
        # if pl_status is True:
        #     break
    # sim.send_structs("MotorController", "TorqueEncoder")

    assert pl_status is True
    print(f"received pl_status: {pl_status}")
    