"""
Power Limit FFI Bindings
========================
This module handles loading the libpl_sil.dll and defining the C function interfaces.
Import `ffi` and `lib` from this module to use in your tests.
"""

from cffi import FFI
import os
import sys

# Initialize FFI
ffi = FFI()

# Get absolute path to DLL
script_dir = os.path.dirname(os.path.abspath(__file__))
lib_path = os.path.join(script_dir, "libpl_sil.dll")

# Define C interface
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

# Load the DLL
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

