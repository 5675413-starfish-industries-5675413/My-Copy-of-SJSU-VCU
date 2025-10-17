from cffi import FFI
import pytest
import math

ffi = FFI()
import os
import sys

# Get absolute path to DLL
script_dir = os.path.dirname(os.path.abspath(__file__))
lib_path = os.path.join(script_dir, "libpl_sil.dll")

print(f"Loading DLL from: {lib_path}")
print(f"DLL exists: {os.path.exists(lib_path)}")

try:
    lib = ffi.dlopen(lib_path)
    print("OK: DLL loaded successfully!")
except OSError as e:
    print(f"ERROR: Failed to load DLL: {e}")
    print("\nTrying alternative: load with ctypes first...")
    import ctypes
    try:
        ctypes.CDLL(lib_path)
        print("ctypes can load it - retrying with cffi...")
        lib = ffi.dlopen(lib_path)
    except Exception as e2:
        print(f"ctypes also failed: {e2}")
        sys.exit(1)

ffi.cdef(r"""
typedef unsigned char  uint8_t;
typedef signed char    int8_t;
typedef short          int16_t;
typedef int            int32_t;
typedef _Bool          bool;

typedef struct SerialManager       SerialManager;
typedef struct MotorController     MotorController;
typedef struct PowerLimit          PowerLimit;
typedef struct TorqueEncoder       TorqueEncoder;
typedef struct BrakePressureSensor BusPower;  /* BusPower is alias for BrakePressureSensor */

/* REAL constructors */
SerialManager*   SerialManager_new(void);
MotorController* MotorController_new(SerialManager* sm, int id, int dir, int a, int b, int c);
PowerLimit*      POWERLIMIT_new(bool plToggle);
TorqueEncoder*   TorqueEncoder_new(bool bench);

/* Functions under test */
void MCM_calculateCommands(MotorController* mcm0, TorqueEncoder* tps, BusPower* bps);
void PowerLimit_calculateCommand(PowerLimit* pl, MotorController* mcm0, TorqueEncoder* tps);

/* TorqueEncoder Test Helpers */
void    TEST_TPS_set0_percent(TorqueEncoder* te, float percent);
void    TEST_TPS_set1_percent(TorqueEncoder* te, float percent);
void    TEST_TPS_setBoth_percent(TorqueEncoder* te, float percent);
void    TEST_TPS_setTravelPercent(TorqueEncoder* te);
float   TEST_getTPS0_percent(TorqueEncoder* te);
float   TEST_getTPS1_percent(TorqueEncoder* te);
float   TEST_getTravelPercent(TorqueEncoder* te);

/* Motor Controller Test Helpers */
void    TEST_MCM_setCommandedTorque(MotorController* me);
void    TEST_MCM_setDCVoltage(MotorController* me, int32_t voltage);
void    TEST_MCM_setDCCurrent(MotorController* me, int32_t current);
void    TEST_MCM_setRPM(MotorController* me, int32_t motorRPM);
int32_t TEST_MCM_getDCVoltage(MotorController* me);
int32_t TEST_MCM_getDCCurrent(MotorController* me);
int32_t TEST_MCM_getMotorRPM(MotorController* me);
int16_t TEST_MCM_getAppsTorque(MotorController* me);

/* Sensor Test Helpers */
void    TEST_setPLKnobVoltage(int32_t voltage);

/* Power Limit Test Helpers */
void    TEST_setPLAlwaysOn(PowerLimit* me, bool alwaysOn);
void    TEST_setPID(PowerLimit* me, int8_t kp, int8_t ki, int8_t kd, int16_t saturationValue, int16_t scalefactor);
void    TEST_setPLMode(PowerLimit* me, uint8_t mode);
void    TEST_setPLStatus(PowerLimit* me, bool status);
void    TEST_setPLTorqueCommand(PowerLimit* me, int16_t plTorqueCommand);
void    TEST_setPLTargetPower(PowerLimit* me, uint8_t targetPower);
void    TEST_setPLThresholdDiscrepancy(PowerLimit* me, uint8_t thresholdDiscrepancy);
void    TEST_setPLInitializationThreshold(PowerLimit* me, uint8_t initializationThreshold);
void    TEST_setClampingMethod(PowerLimit* me, uint8_t clampingMethod);
int16_t TEST_getPLTorqueCommand(PowerLimit* me);
bool    TEST_getPLAlwaysOn(PowerLimit* me);
uint8_t TEST_getPLMode(PowerLimit* me);
bool    TEST_getPLStatus(PowerLimit* me);
uint8_t TEST_getPLTargetPower(PowerLimit* me);
uint8_t TEST_getPLThresholdDiscrepancy(PowerLimit* me);
uint8_t TEST_getPLInitializationThreshold(PowerLimit* me);
uint8_t TEST_getPLClampingMethod(PowerLimit* me);
""")


# -------- Helper functions --------
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
def test_one_step_runs(pl, mcm0, tps):
    # Set TPS to 50%
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
    lib.TEST_setPLAlwaysOn(pl, True)
    lib.TEST_setPID(pl, 10, 50, 0, 231, 10)
    lib.TEST_setPLMode(pl, 1)
    lib.TEST_setPLStatus(pl, True)
    lib.TEST_setPLTorqueCommand(pl, 0)
    lib.TEST_setPLTargetPower(pl, 40)
    lib.TEST_setPLThresholdDiscrepancy(pl, 15)
    lib.TEST_setPLInitializationThreshold(pl, 0)
    lib.TEST_setClampingMethod(pl, 6)
    
    # Debug: Print values before PL calculation
    print("\n=== DEBUG: Before PowerLimit_calculateCommand ===")
    print(f"TPS0: {lib.TEST_getTPS0_percent(tps):.2f}, TPS1: {lib.TEST_getTPS1_percent(tps):.2f}, Travel: {lib.TEST_getTravelPercent(tps):.2f}")
    print(f"MCM RPM: {lib.TEST_MCM_getMotorRPM(mcm0)}, Voltage: {lib.TEST_MCM_getDCVoltage(mcm0)}V, Current: {lib.TEST_MCM_getDCCurrent(mcm0)}A")
    print(f"MCM appsTorque: {lib.TEST_MCM_getAppsTorque(mcm0)}")
    print(f"PL Status (before): {lib.TEST_getPLStatus(pl)}, Mode: {lib.TEST_getPLMode(pl)}, AlwaysOn: {lib.TEST_getPLAlwaysOn(pl)}")
    print(f"PL TargetPower: {lib.TEST_getPLTargetPower(pl)}kW, InitThreshold: {lib.TEST_getPLInitializationThreshold(pl)}kW")
    
    lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    
    # Debug: Print values after PL calculation
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

if __name__ == "__main__":
    pytest.main([__file__, "-v"])
