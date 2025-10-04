from cffi import FFI
import pytest

ffi = FFI()
lib = ffi.dlopen("./libpl_sil.so")  # or .dylib/.dll

ffi.cdef(r"""
typedef unsigned char  uint8_t;
typedef short          int16_t;
typedef _Bool          bool;

typedef struct SerialManager   SerialManager;
typedef struct MotorController MotorController;
typedef struct PowerLimit      PowerLimit;
typedef struct TorqueEncoder   TorqueEncoder;

/* REAL constructors (or use the TEST_* wrappers if you prefer) */
SerialManager*   SerialManager_new(void);
MotorController* MotorController_new(SerialManager* sm, int id, int dir, int a, int b, int c);
PowerLimit*      POWERLIMIT_new(bool plToggle);
TorqueEncoder*   TorqueEncoder_new(bool bench);

/* Function under test */
int PowerLimit_calculateCommand(PowerLimit* pl, MotorController* mcm0, TorqueEncoder* tps);

/* Test helpers */
void    TEST_TPS_attachVirtual(TorqueEncoder* te);
void    TEST_TPS_set0_raw(TorqueEncoder* te, int raw);
void    TEST_TPS_set1_raw(TorqueEncoder* te, int raw);
int     TEST_TPS_get0_raw(void);
int     TEST_TPS_get1_raw(void);

void    TEST_MCM_setRPM(MotorController* mcm, int rpm);
void    TEST_MCM_setBusDeciVolts(MotorController* mcm, int dv);

void    TEST_PL_setTargetKW(PowerLimit* pl, uint8_t kw);
void    TEST_PL_setMode(PowerLimit* pl, uint8_t mode);
void    TEST_PL_setAlwaysOn(PowerLimit* pl, bool on);
void    TEST_PL_setToggle(PowerLimit* pl, bool on);
int16_t TEST_PL_getTorqueCmd(const PowerLimit* pl);
bool    TEST_PL_getStatus(const PowerLimit* pl);
""")


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
def tps(request, serialMan):
    m = lib.TorqueEncoder_new(serialMan, 0xA0, 1, 2310, 5, 10)
    assert m != ffi.NULL
    return m

@pytest.fixture
def pl(request):
    p = lib.POWERLIMIT_new(True)
    assert p != ffi.NULL
    return p
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
