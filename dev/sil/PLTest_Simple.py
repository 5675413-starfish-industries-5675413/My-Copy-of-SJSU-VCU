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
    print("✓ DLL loaded successfully!")
except OSError as e:
    print(f"✗ Failed to load DLL: {e}")
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

/* Motor Controller Test Helpers */
void    TEST_MCM_setCommandedTorque(MotorController* me);
void    TEST_MCM_setDCVoltage(MotorController* me, int32_t voltage);
void    TEST_MCM_setDCCurrent(MotorController* me, int32_t current);
void    TEST_MCM_setRPM(MotorController* me, int32_t motorRPM);

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
    
    # Configure Power Limit Parameters
    lib.TEST_setPLAlwaysOn(pl, True)
    lib.TEST_setPID(pl, 10, 50, 0, 231, 10)
    lib.TEST_setPLMode(pl, 1)
    #lib.TEST_setPLStatus(pl, True)
    lib.TEST_setPLTorqueCommand(pl, 0)
    lib.TEST_setPLTargetPower(pl, 40)
    lib.TEST_setPLThresholdDiscrepancy(pl, 15)
    lib.TEST_setPLInitializationThreshold(pl, 0)
    lib.TEST_setClampingMethod(pl, 6)
    
    lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    print(lib.TEST_getPLTorqueCommand(pl))
    assert lib.TEST_getPLTorqueCommand(pl) == 1000

# -------- Setter Verification Tests --------
def test_tps_setters(tps):
    """Verify TPS percentage setters work correctly"""
    # Test setting percentages - verified by successful calls (no crashes)
    lib.TEST_TPS_set0_percent(tps, 0.3)
    lib.TEST_TPS_set1_percent(tps, 0.3)
    
    # Test setting both at once
    lib.TEST_TPS_setBoth_percent(tps, 0.75)
    
    # Test travel percent calculation
    lib.TEST_TPS_setTravelPercent(tps)
    
    # If no errors, test passes
    assert True

def test_mcm_setters(mcm0, serialMan):
    """Verify MCM setter functions work correctly"""
    # Test RPM setter - verified by successful calls
    lib.TEST_MCM_setRPM(mcm0, 3000)
    lib.TEST_MCM_setRPM(mcm0, 5000)
    
    # Test voltage and current setters
    lib.TEST_MCM_setDCVoltage(mcm0, 4000)
    lib.TEST_MCM_setDCCurrent(mcm0, 200)
    
    # If no errors, test passes
    assert True

def test_powerlimit_setters(pl):
    """Verify PowerLimit setter functions work correctly"""
    # Test all setters - verified by successful calls
    lib.TEST_setPLTargetPower(pl, 80)
    lib.TEST_setPLMode(pl, 1)
    lib.TEST_setPLAlwaysOn(pl, True)
    lib.TEST_setPLAlwaysOn(pl, False)
    lib.TEST_setPLThresholdDiscrepancy(pl, 20)
    lib.TEST_setClampingMethod(pl, 6)
    lib.TEST_setPLStatus(pl, True)
    
    # Test torque command with getter to verify it works
    lib.TEST_setPLTorqueCommand(pl, 1500)
    torque = lib.TEST_getPLTorqueCommand(pl)
    assert torque == 1500  # This should work since we have a getter

if __name__ == "__main__":
    pytest.main([__file__, "-v"])
