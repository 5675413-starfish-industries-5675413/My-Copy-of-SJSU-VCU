"""
Power Limit Test Suite
======================
Tests for the Power Limit module using Software-in-the-Loop (SIL) testing.
"""

import pytest
import os
import json

# Import FFI bindings (this loads the DLL and defines all C interfaces)
from pl_exported_functions import ffi, lib

# Get script directory for loading config files
script_dir = os.path.dirname(os.path.abspath(__file__))


# -------- Helper functions --------
def load_power_limit_configs():
    """Load configurations from power_limit_params.json"""
    json_path = os.path.join(script_dir, "easyCompiling", "power_limit_params.json")
    with open(json_path, 'r') as f:
        data = json.load(f)
    return data['build_configurations']

def set_tps_percent(tps, percent):
    """Set TPS percentages directly (0.0 to 1.0)"""
    lib.TEST_TPS_setBoth_percent(tps, percent)
    lib.TEST_TPS_setTravelPercent(tps)

# -------- fixtures (new objects per test) --------
@pytest.fixture
def serialMan(request):
    sm = lib.SerialManager_new()
    assert sm != ffi.NULL
    return sm

@pytest.fixture
def mcm0(request, serialMan):
    m = lib.MotorController_new(serialMan, 0xA0, 1, 2310, 5, 10)
    assert m != ffi.NULL
    return m

@pytest.fixture
def tps(request):
    t = lib.TorqueEncoder_new(False)
    assert t != ffi.NULL
    return t

@pytest.fixture
def pl(request):
    p = lib.POWERLIMIT_new(True)
    assert p != ffi.NULL
    return p


# -------- Basic functionality tests --------
@pytest.mark.parametrize("config", load_power_limit_configs(), ids=lambda c: c['name'])
def test_one_step_runs_parameterized(pl, mcm0, tps, config):
    """Run test with configuration from JSON"""
    params = config['parameters']
    pid = params['pid']
    
    # Set TPS to 100%
    set_tps_percent(tps, 1.0)
    
    # Configure Motor Controller Parameters
    lib.TEST_MCM_setCommandedTorque(mcm0)
    lib.TEST_MCM_setRPM(mcm0, 3000)
    lib.TEST_MCM_setDCVoltage(mcm0, 700)
    lib.TEST_MCM_setDCCurrent(mcm0, 50)
    lib.MCM_calculateCommands(mcm0, tps, ffi.NULL)
    
    # Configure PL Knob Sensor (must be set for getPLMode() to work!)
    # For PL_MODE_40: voltage must be between 1500-2300
    lib.TEST_setPLKnobVoltage(1800)  # Select 40kW mode
    
    # Configure Power Limit Parameters
    lib.TEST_setPLAlwaysOn(pl, params['plAlwaysOn'])
    lib.TEST_setPID(pl, pid['Kp'], pid['Ki'], pid['Kd'], pid['saturation'], pid['gain'])
    lib.TEST_setPLMode(pl, params['plMode'])
    lib.TEST_setPLStatus(pl, True)
    lib.TEST_setPLTorqueCommand(pl, 0)
    lib.TEST_setPLTargetPower(pl, params['plTargetPower'])
    lib.TEST_setPLThresholdDiscrepancy(pl, params['plThresholdDiscrepancy'])
    lib.TEST_setPLInitializationThreshold(pl, params['plInitializationThreshold'])
    lib.TEST_setClampingMethod(pl, params['clampingMethod'])
    
    # Print values before PL calculation
    print("\n=== DEBUG: Before PowerLimit_calculateCommand ===")
    print(f"TPS0: {lib.TEST_getTPS0_percent(tps):.2f}\nTPS1: {lib.TEST_getTPS1_percent(tps):.2f}\nTravel: {lib.TEST_getTravelPercent(tps):.2f}")
    print(f"MCM RPM: {lib.TEST_MCM_getMotorRPM(mcm0)}\nVoltage: {lib.TEST_MCM_getDCVoltage(mcm0)}V\nCurrent: {lib.TEST_MCM_getDCCurrent(mcm0)}A")
    print(f"MCM appsTorque: {lib.TEST_MCM_getAppsTorque(mcm0)}")
    print(f"PL Status (before): {lib.TEST_getPLStatus(pl)}\nMode: {lib.TEST_getPLMode(pl)}\nAlwaysOn: {lib.TEST_getPLAlwaysOn(pl)}")
    print(f"PL TargetPower: {lib.TEST_getPLTargetPower(pl)}kW\nInitThreshold: {lib.TEST_getPLInitializationThreshold(pl)}kW")
    
    lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    
    # Print values after PL calculation
    print("\n=== DEBUG: After PowerLimit_calculateCommand ===")
    print(f"PL Status (after): {lib.TEST_getPLStatus(pl)}")
    print(f"PL TorqueCommand: {lib.TEST_getPLTorqueCommand(pl)}")
    
    # Verify PL is active and producing a torque command
    assert lib.TEST_getPLStatus(pl) == True, "PowerLimit should be active"
    assert lib.TEST_getPLTorqueCommand(pl) > 0, "PowerLimit should produce non-zero torque command"
    # Actual value is ~1270 with these parameters
    
def test_one_step_runs(pl, mcm0, tps):
    # Set TPS to 100%
    set_tps_percent(tps, 1.0)
    
    # Configure Motor Controller Parameters
    lib.TEST_MCM_setCommandedTorque(mcm0)
    lib.TEST_MCM_setRPM(mcm0, 2441)
    lib.TEST_MCM_setDCVoltage(mcm0, 900)
    lib.TEST_MCM_setDCCurrent(mcm0, 50)
    lib.MCM_calculateCommands(mcm0, tps, ffi.NULL)
    
    # Configure PL Knob Sensor (must be set for getPLMode() to work!)
    # For PL_MODE_40: voltage must be between 1500-2300
    lib.TEST_setPLKnobVoltage(1800)  # Select 40kW mode
    
    # Configure Power Limit Parameters
    lib.TEST_setPLStatus(pl, True)
    lib.TEST_setPLAlwaysOn(pl, 1)
    lib.TEST_setPID(pl, 10, 10, 0, 231, 10)
    lib.TEST_setPLMode(pl, 2)
    lib.TEST_setPLStatus(pl, True)
    #lib.TEST_setPLTorqueCommand(pl, 0)
    lib.TEST_setPLTargetPower(pl, 40)
    lib.TEST_setPLThresholdDiscrepancy(pl, 5)
    lib.TEST_setPLInitializationThreshold(pl, 0)
    lib.TEST_setClampingMethod(pl, 4)
    
    # Print values before PL calculation
    print("\n=== DEBUG: Before PowerLimit_calculateCommand ===")
    print(f"TPS0: {lib.TEST_getTPS0_percent(tps):.2f}\nTPS1: {lib.TEST_getTPS1_percent(tps):.2f}\nTravel: {lib.TEST_getTravelPercent(tps):.2f}")
    print(f"MCM RPM: {lib.TEST_MCM_getMotorRPM(mcm0)}\nVoltage: {lib.TEST_MCM_getDCVoltage(mcm0)}V\nCurrent: {lib.TEST_MCM_getDCCurrent(mcm0)}A")
    print(f"MCM appsTorque: {lib.TEST_MCM_getAppsTorque(mcm0)}")
    print(f"PL Status (before): {lib.TEST_getPLStatus(pl)}\nMode: {lib.TEST_getPLMode(pl)}\nAlwaysOn: {lib.TEST_getPLAlwaysOn(pl)}")
    print(f"PL TargetPower: {lib.TEST_getPLTargetPower(pl)}kW\nInitThreshold: {lib.TEST_getPLInitializationThreshold(pl)}kW")
    
    lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    
    # Print values after PL calculation
    print("\n=== DEBUG: After PowerLimit_calculateCommand ===")
    print(f"PL Status (after): {lib.TEST_getPLStatus(pl)}")
    print(f"PL TorqueCommand: {lib.TEST_getPLTorqueCommand(pl)}")
    
    # Verify PL is active and producing a torque command
    assert lib.TEST_getPLStatus(pl) == True, "PowerLimit should be active"
    assert lib.TEST_getPLTorqueCommand(pl) > 0, "PowerLimit should produce non-zero torque command"
    # Actual value is ~1270 with these parameters

# -------- Setter Verification Tests --------
def test_tps_setters(tps):
    """Verify TPS percentage setters work correctly"""
    # Test setting individual percentages
    lib.TEST_TPS_set0_percent(tps, 0.3)
    assert lib.TEST_getTPS0_percent(tps) == pytest.approx(0.3, abs=0.01)
    
    lib.TEST_TPS_set1_percent(tps, 0.4)
    assert lib.TEST_getTPS1_percent(tps) == pytest.approx(0.4, abs=0.01)
    
    # Test setting both at once
    lib.TEST_TPS_setBoth_percent(tps, 0.75)
    assert lib.TEST_getTPS0_percent(tps) == pytest.approx(0.75, abs=0.01)
    assert lib.TEST_getTPS1_percent(tps) == pytest.approx(0.75, abs=0.01)
    
    # Test travel percent calculation
    lib.TEST_TPS_set0_percent(tps, 0.5)
    lib.TEST_TPS_set1_percent(tps, 0.7)
    lib.TEST_TPS_setTravelPercent(tps)
    assert lib.TEST_getTravelPercent(tps) == pytest.approx(0.6, abs=0.01)  # (0.5 + 0.7) / 2

def test_mcm_setters(mcm0, serialMan):
    """Verify MCM setter functions work correctly"""
    # Test RPM setter (note: these are increments, not absolute sets)
    initial_rpm = lib.TEST_MCM_getMotorRPM(mcm0)
    lib.TEST_MCM_setRPM(mcm0, 3000)
    assert lib.TEST_MCM_getMotorRPM(mcm0) == initial_rpm + 3000
    
    lib.TEST_MCM_setRPM(mcm0, 2000)
    assert lib.TEST_MCM_getMotorRPM(mcm0) == initial_rpm + 5000
    
    # Test voltage setter (incremental with 1000V cap)
    initial_voltage = lib.TEST_MCM_getDCVoltage(mcm0)
    lib.TEST_MCM_setDCVoltage(mcm0, 100)  # Add 100V, should not exceed cap
    expected_voltage = min(initial_voltage + 100, 1000)
    assert lib.TEST_MCM_getDCVoltage(mcm0) == expected_voltage
    
    # Test current setter (incremental with 500A cap)
    initial_current = lib.TEST_MCM_getDCCurrent(mcm0)
    lib.TEST_MCM_setDCCurrent(mcm0, 50)  # Add 50A, should not exceed cap
    expected_current = min(initial_current + 50, 500)
    assert lib.TEST_MCM_getDCCurrent(mcm0) == expected_current

def test_powerlimit_setters(pl):
    """Verify PowerLimit setter functions work correctly"""
    # Test target power
    lib.TEST_setPLTargetPower(pl, 80)
    assert lib.TEST_getPLTargetPower(pl) == 80
    
    # Test mode
    lib.TEST_setPLMode(pl, 1)
    assert lib.TEST_getPLMode(pl) == 1
    
    # Test always on (toggle between true and false)
    lib.TEST_setPLAlwaysOn(pl, True)
    assert lib.TEST_getPLAlwaysOn(pl) == True
    
    lib.TEST_setPLAlwaysOn(pl, False)
    assert lib.TEST_getPLAlwaysOn(pl) == False
    
    # Test threshold discrepancy
    lib.TEST_setPLThresholdDiscrepancy(pl, 20)
    assert lib.TEST_getPLThresholdDiscrepancy(pl) == 20
    
    # Test initialization threshold
    lib.TEST_setPLInitializationThreshold(pl, 5)
    assert lib.TEST_getPLInitializationThreshold(pl) == 5
    
    # Test clamping method
    lib.TEST_setClampingMethod(pl, 6)
    assert lib.TEST_getPLClampingMethod(pl) == 6
    
    # Test status
    lib.TEST_setPLStatus(pl, True)
    assert lib.TEST_getPLStatus(pl) == True
    
    # Test torque command
    lib.TEST_setPLTorqueCommand(pl, 1500)
    assert lib.TEST_getPLTorqueCommand(pl) == 1500

# -------- Rotary (PL Knob) Tests --------
@pytest.mark.parametrize("voltage,expected_power,description", [
    (4000, 80, "PL_MODE_80 (>3700V)"),
    (3000, 60, "PL_MODE_60 (2901-3700V)"),
    (2500, 50, "PL_MODE_50 (2301-2900V)"),
    (1800, 40, "PL_MODE_40 (1501-2300V)"),
    (1200, 30, "PL_MODE_30 (801-1500V)"),
    (500, 0, "PL_MODE_OFF (<=800V)"),
])
def test_rotary_voltage_ranges(pl, mcm0, tps, voltage, expected_power, description):
    """Test that rotary knob voltage correctly maps to power limit modes"""
    
    # Set up test conditions
    set_tps_percent(tps, 1.0)  # Full throttle
    lib.TEST_MCM_setCommandedTorque(mcm0)
    lib.TEST_MCM_setRPM(mcm0, 3000)
    lib.TEST_MCM_setDCVoltage(mcm0, 700)
    lib.TEST_MCM_setDCCurrent(mcm0, 50)
    lib.MCM_calculateCommands(mcm0, tps, ffi.NULL)
    
    # Set rotary knob voltage
    lib.TEST_setPLKnobVoltage(voltage)
    
    # Configure basic PL parameters
    lib.TEST_setPLAlwaysOn(pl, False)  # Let rotary control toggle
    lib.TEST_setPID(pl, 10, 0, 0, 231, 10)
    lib.TEST_setPLMode(pl, 1)
    lib.TEST_setPLStatus(pl, False)
    lib.TEST_setPLTorqueCommand(pl, 0)
    lib.TEST_setPLThresholdDiscrepancy(pl, 15)
    lib.TEST_setClampingMethod(pl, 3)
    
    # Run power limit calculation (this reads rotary and sets target power)
    lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    
    # Verify the target power was set correctly (except for OFF mode)
    if expected_power > 0:
        actual_power = lib.TEST_getPLTargetPower(pl)
        print(f"\n{description}: Voltage={voltage}V -> Expected={expected_power}kW, Got={actual_power}kW")
        assert actual_power == expected_power, f"Voltage {voltage}V should map to {expected_power}kW, got {actual_power}kW"
        
        # For power modes where motor power (~44kW) is below target, PL status will be True
        # For 60kW and 80kW modes, motor is already below limit so PL may not need to activate
        if expected_power <= 50:
            assert lib.TEST_getPLStatus(pl) == True, f"PowerLimit should be active at {voltage}V ({expected_power}kW)"
    else:
        # OFF mode - just verify that PL is disabled (status should be False)
        print(f"\n{description}: Voltage={voltage}V -> PL should be OFF")
        assert lib.TEST_getPLStatus(pl) == False, f"PowerLimit should be OFF at {voltage}V"
        
def test_plstatus(pl, mcm0, tps):
    lib.TEST_setPLStatus(pl, True)

    assert lib.TEST_getPLStatus(pl) == True, "PowerLimit should be active"
        
if __name__ == "__main__":
    pytest.main([__file__, "-v"])
