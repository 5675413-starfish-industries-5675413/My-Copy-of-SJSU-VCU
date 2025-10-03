from cffi import FFI
import pytest

ffi = FFI()
lib = ffi.dlopen("./libpl_sil.so")  # or .dylib/.dll

ffi.cdef(r"""
#include <stdint.h>
typedef struct SerialManager  SerialManager;
typedef struct MotorController MotorController;
typedef struct PowerLimit     PowerLimit;
typedef struct TorqueEncoder  TorqueEncoder;

// REAL constructors (or use the TEST_* wrappers if you prefer)
SerialManager*  SerialManager_new(void);
MotorController* MotorController_new(SerialManager* sm, int id, int dir, int a, int b, int c);
PowerLimit*     POWERLIMIT_new(_Bool plToggle);
TorqueEncoder*  TorqueEncoder_new(_Bool bench);

void SerialManager_free(SerialManager*);
void MotorController_free(MotorController*);
void POWERLIMIT_free(PowerLimit*);
void TorqueEncoder_free(TorqueEncoder*);

// Function under test
int PowerLimit_calculateCommand(PowerLimit* pl, MotorController* mcm0, TorqueEncoder* tps);

// Test helpers
void    TEST_TPS_attachVirtual(TorqueEncoder* te);
void    TEST_TPS_set0_raw(TorqueEncoder* te, int raw);
void    TEST_TPS_set1_raw(TorqueEncoder* te, int raw);
int     TEST_TPS_get0_raw(void);
int     TEST_TPS_get1_raw(void);

void    TEST_MCM_setRPM(MotorController* mcm, int rpm);
void    TEST_MCM_setBusDeciVolts(MotorController* mcm, int dv);

void    TEST_PL_setTargetKW(PowerLimit* pl, uint8_t kw);
void    TEST_PL_setMode(PowerLimit* pl, uint8_t mode);
void    TEST_PL_setAlwaysOn(PowerLimit* pl, _Bool on);
void    TEST_PL_setToggle(PowerLimit* pl, _Bool on);
int16_t TEST_PL_getTorqueCmd(const PowerLimit* pl);
_Bool   TEST_PL_getStatus(const PowerLimit* pl);
""")

# -------- fixtures (new objects per test) --------
@pytest.fixture
def serialMan(request):
    sm = lib.SerialManager_new()
    assert sm != ffi.NULL
    request.addfinalizer(lambda: lib.SerialManager_free(sm))
    return sm

@pytest.fixture
def mcm0(request, serialMan):
    # Match your real ctor params: id, direction, etc.
    m = lib.MotorController_new(serialMan, 0xA0, 1, 2310, 5, 10)  # FORWARD==1?
    assert m != ffi.NULL
    request.addfinalizer(lambda: lib.MotorController_free(m))
    return m

@pytest.fixture
def pl(request):
    p = lib.POWERLIMIT_new(True)
    assert p != ffi.NULL
    request.addfinalizer(lambda: lib.POWERLIMIT_free(p))
    return p

@pytest.fixture
def tps(request):
    t = lib.TorqueEncoder_new(True)
    assert t != ffi.NULL
    request.addfinalizer(lambda: lib.TorqueEncoder_free(t))
    # route to virtual sensors once
    lib.TEST_TPS_attachVirtual(t)
    return t

# -------- first test: one step smoke --------
def test_one_step_runs(pl, mcm0, tps):
    lib.TEST_PL_setToggle(pl, True)
    lib.TEST_PL_setAlwaysOn(pl, True)
    lib.TEST_PL_setMode(pl, 1)         # 1 = Torque-PID (per your comment)
    lib.TEST_PL_setTargetKW(pl, 30)    # kW

    lib.TEST_MCM_setRPM(mcm0, 2500)
    lib.TEST_MCM_setBusDeciVolts(mcm0, 6500)  # 650.0 V

    lib.TEST_TPS_set0_raw(tps, 1200)
    lib.TEST_TPS_set1_raw(tps, 1180)

    rc = lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    assert rc == 0

    cmd = lib.TEST_PL_getTorqueCmd(pl)
    on  = lib.TEST_PL_getStatus(pl)
    assert on in (True, False)
    assert -32768 <= cmd <= 32767

# -------- loop test: update sensors every tick --------
def test_loop_updates_inputs(pl, mcm0, tps):
    lib.TEST_PL_setToggle(pl, True)
    lib.TEST_PL_setAlwaysOn(pl, True)
    lib.TEST_PL_setMode(pl, 1)
    lib.TEST_PL_setTargetKW(pl, 30)

    lib.TEST_MCM_setRPM(mcm0, 2500)
    lib.TEST_MCM_setBusDeciVolts(mcm0, 6500)

    measured = 0
    for k in range(100):
        # Change TPS each loop (simulate motion / noise)
        lib.TEST_TPS_set0_raw(tps, 1100 + k)
        lib.TEST_TPS_set1_raw(tps, 1100 + k//2)

        rc = lib.PowerLimit_calculateCommand(pl, mcm0, tps)
        assert rc == 0

        cmd = lib.TEST_PL_getTorqueCmd(pl)
        assert -32768 <= cmd <= 32767

        # If your controller reads measured torque back from TPS,
        # you can feed back 'measured' here via another setter.
        measured = (measured*7 + cmd) // 8  # simple first-order lag
