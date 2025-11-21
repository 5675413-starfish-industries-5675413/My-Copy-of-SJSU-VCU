"""
Regen FFI Bindings
==================
This module handles loading the libpl_sil.dll and defining the C function interfaces that
`regenTest.py` will use.
Import `ffi` and `lib` from this module to call into the SIL helper DLL.
"""

from cffi import FFI  # type: ignore[import]
import os
import sys

# Initialize FFI
ffi = FFI()

# Get absolute path to DLL
script_dir = os.path.dirname(os.path.abspath(__file__))
lib_path = os.path.join(script_dir, "dll", "libpl_sil.dll")

# Define C interface
ffi.cdef(r"""
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef signed char    int8_t;
typedef short          int16_t;
typedef int            int32_t;
typedef _Bool          bool;

typedef enum { REGENMODE_FORMULAE, REGENMODE_TESLA, REGENMODE_HYBRID, REGENMODE_FIXED, REGENMODE_CUSTOM, REGENMODE_OFF } RegenMode;

typedef struct SerialManager SerialManager;
typedef struct MotorController MotorController;
typedef struct PowerLimit PowerLimit;
typedef struct TorqueEncoder TorqueEncoder;
typedef struct BrakePressureSensor BrakePressureSensor;
typedef struct Regen Regen;

SerialManager* SerialManager_new(void);
MotorController* MotorController_new(SerialManager* sm, uint16_t id, int dir, int a, int b, int c);
PowerLimit* POWERLIMIT_new(bool plToggle);
TorqueEncoder* TorqueEncoder_new(bool bench);
BrakePressureSensor* BrakePressureSensor_new(void);
Regen* Regen_new(bool regenToggle);

void Regen_calculateCommands(Regen* me, MotorController* mcm, TorqueEncoder* tps, BrakePressureSensor* bps);
void Regen_updateSettings(Regen* me);
int16_t Regen_get_torqueCommand(Regen* me);

void MCM_TEST_setCommandedTorque(MotorController* me);
void MCM_incrementVoltageForTesting(MotorController* me, int32_t increment);
void MCM_incrementCurrentForTesting(MotorController* me, int32_t increment);
void MCM_incrementRPMForTesting(MotorController* me, int32_t increment);

int32_t TEST_MCM_getDCVoltage(MotorController* me);
int32_t TEST_MCM_getDCCurrent(MotorController* me);
int32_t TEST_MCM_getMotorRPM(MotorController* me);

float TEST_Regen_getAppsTorque(Regen* me);
float TEST_Regen_getBpsTorqueNm(Regen* me);
int16_t TEST_Regen_getTorqueLimitDNm(Regen* me);
float TEST_Regen_getPadMu(Regen* me);
RegenMode TEST_Regen_getMode(Regen* me);

void TEST_Regen_setAppsTorque(Regen* me, float appsTorque);
void TEST_Regen_setBpsTorqueNm(Regen* me, float bpsTorqueNm);
void TEST_Regen_setTorqueLimitDNm(Regen* me, int16_t torqueLimitDNm);
void TEST_Regen_setPadMu(Regen* me, float padMu);
void TEST_Regen_setMode(Regen* me, RegenMode mode);

float TEST_TorqueEncoder_getOutputPercent(TorqueEncoder* me);
void TEST_BPS_setPressure(BrakePressureSensor* me);

float TEST_MCM_getGroundSpeedKPH(MotorController* me);
void TEST_MCM_setGroundSpeedKPH(MotorController* me, float groundSpeedKPH);
void TEST_MCM_setMotorRPMValue(MotorController* me, int32_t motorRPM);
void TEST_MCM_setDCCurrentValue(MotorController* me, int32_t current);

uint16_t TEST_getMinRegenSpeedKPH(void);
void TEST_setMinRegenSpeedKPH(uint16_t minRegenSpeedKPH);
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
        raise ImportError(f"Failed to load DLL {lib_path}: {e2}") from e2

