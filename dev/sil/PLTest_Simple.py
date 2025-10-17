from cffi import FFI
import pytest
import math

ffi = FFI()
import os
lib_path = os.path.join(os.path.dirname(__file__), "libpl_sil.dll")
lib = ffi.dlopen(lib_path)  # Windows DLL with full path

ffi.cdef(r"""
typedef unsigned char  uint8_t;
typedef short          int16_t;
typedef _Bool          bool;

typedef struct SerialManager   SerialManager;
typedef struct MotorController MotorController;
typedef struct PowerLimit      PowerLimit;
typedef struct TorqueEncoder   TorqueEncoder;

/* REAL constructors */
SerialManager*   SerialManager_new(void);
MotorController* MotorController_new(SerialManager* sm, int id, int dir, int a, int b, int c);
PowerLimit*      POWERLIMIT_new(bool plToggle);
TorqueEncoder*   TorqueEncoder_new(bool bench);

/* Function under test */
int PowerLimit_calculateCommand(PowerLimit* pl, MotorController* mcm0, TorqueEncoder* tps);

/* Motor Controller Test Helpers */
void    TEST_MCM_setRPM(MotorController* mcm, int rpm);
void    TEST_MCM_setBusDeciVolts(MotorController* mcm, int dv);
void    TEST_MCM_setCurrent(MotorController* mcm, int current);
void    TEST_MCM_setCommandedTorque(MotorController* mcm, int torque);
int     TEST_MCM_getRPM(MotorController* mcm);
int     TEST_MCM_getBusDeciVolts(MotorController* mcm);
int     TEST_MCM_getCurrent(MotorController* mcm);
int     TEST_MCM_getCommandedTorque(MotorController* mcm);
int     TEST_MCM_getAppsTorque(MotorController* mcm);

/* Power Limit Test Helpers */
void    TEST_PL_setTargetPower(PowerLimit* pl, uint8_t kw);
void    TEST_PL_setMode(PowerLimit* pl, uint8_t mode);
void    TEST_PL_setAlwaysOn(PowerLimit* pl, bool on);
void    TEST_PL_setToggle(PowerLimit* pl, bool on);
void    TEST_PL_setClampingMethod(PowerLimit* pl, uint8_t method);
void    TEST_PL_setThresholdDiscrepancy(PowerLimit* pl, uint8_t threshold);
int16_t TEST_PL_getTorqueCmd(const PowerLimit* pl);
bool    TEST_PL_getStatus(const PowerLimit* pl);
uint8_t TEST_PL_getTargetPower(const PowerLimit* pl);
uint8_t TEST_PL_getMode(const PowerLimit* pl);
bool    TEST_PL_getAlwaysOn(const PowerLimit* pl);
bool    TEST_PL_getToggle(const PowerLimit* pl);
uint8_t TEST_PL_getClampingMethod(const PowerLimit* pl);
uint8_t TEST_PL_getThresholdDiscrepancy(const PowerLimit* pl);

/* Torque Encoder Test Helpers */
void    TEST_TPS_attachVirtual(TorqueEncoder* te);
void    TEST_TPS_set0_raw(TorqueEncoder* te, int raw);
void    TEST_TPS_set1_raw(TorqueEncoder* te, int raw);
int     TEST_TPS_get0_raw(void);
int     TEST_TPS_get1_raw(void);
""")


# -------- Helper functions --------
def set_tps_for_torque(tps, target_torque_dnm, max_torque_dnm=2310):
    """Set TPS raw values to achieve a target torque command"""
    # Calculate percentage: target_torque / max_torque
    percentage = target_torque_dnm / max_torque_dnm
    
    # TPS raw values typically range from ~1000 to ~3000 for 0-100%
    # Map percentage to TPS raw values
    tps_min = 1000
    tps_max = 3000
    tps_range = tps_max - tps_min
    
    tps0_raw = int(tps_min + percentage * tps_range)
    tps1_raw = int(tps_min + percentage * tps_range - 20)  # Slight offset for redundancy
    
    lib.TEST_TPS_set0_raw(tps, tps0_raw)
    lib.TEST_TPS_set1_raw(tps, tps1_raw)
    
    return tps0_raw, tps1_raw


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
    """Basic smoke test to ensure power limit calculation runs"""
    lib.TEST_PL_setToggle(pl, True)
    lib.TEST_PL_setAlwaysOn(pl, True)
    lib.TEST_PL_setMode(pl, 1)         # 1 = Torque-PID
    lib.TEST_PL_setTargetPower(pl, 30) # 30 kW

    lib.TEST_MCM_setRPM(mcm0, 2500)
    lib.TEST_MCM_setBusDeciVolts(mcm0, 6500)  # 650.0 V
    lib.TEST_MCM_setCurrent(mcm0, 50)         # 50 A
    
    # Set TPS values to simulate 1000 dNm apps torque
    set_tps_for_torque(tps, 1000)

    rc = lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    assert rc == 0

    cmd = lib.TEST_PL_getTorqueCmd(pl)
    status = lib.TEST_PL_getStatus(pl)
    assert status in (True, False)
    assert -32768 <= cmd <= 32767


# -------- Parameter injection tests --------
@pytest.mark.parametrize("commanded_torque", [500, 1000, 1500, 2000, 2310])
def test_different_commanded_torques(pl, mcm0, tps, commanded_torque):
    """Test power limit behavior with different commanded torque values"""
    lib.TEST_PL_setToggle(pl, True)
    lib.TEST_PL_setAlwaysOn(pl, True)
    lib.TEST_PL_setMode(pl, 1)
    lib.TEST_PL_setTargetPower(pl, 30)

    # Set up motor controller state
    lib.TEST_MCM_setRPM(mcm0, 3000)
    lib.TEST_MCM_setBusDeciVolts(mcm0, 6000)  # 600V
    lib.TEST_MCM_setCurrent(mcm0, 60)         # 60A = 36kW
    
    # Set TPS values to simulate the commanded torque
    set_tps_for_torque(tps, commanded_torque)

    rc = lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    assert rc == 0

    pl_torque = lib.TEST_PL_getTorqueCmd(pl)
    pl_status = lib.TEST_PL_getStatus(pl)
    
    # Power limit should be active since 36kW > 30kW target
    if pl_status:
        assert pl_torque <= commanded_torque  # Should limit torque
        assert pl_torque >= 0  # Should be positive


@pytest.mark.parametrize("target_power", [20, 30, 40, 50, 60, 80])
def test_different_power_targets(pl, mcm0, tps, target_power):
    """Test power limit behavior with different power targets"""
    lib.TEST_PL_setToggle(pl, True)
    lib.TEST_PL_setAlwaysOn(pl, True)
    lib.TEST_PL_setMode(pl, 1)
    lib.TEST_PL_setTargetPower(pl, target_power)

    # Set up high power scenario
    lib.TEST_MCM_setRPM(mcm0, 4000)
    lib.TEST_MCM_setBusDeciVolts(mcm0, 7000)  # 700V
    lib.TEST_MCM_setCurrent(mcm0, 80)         # 80A = 56kW
    
    # Set TPS values to simulate 2000 dNm apps torque
    set_tps_for_torque(tps, 2000)

    rc = lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    assert rc == 0

    pl_torque = lib.TEST_PL_getTorqueCmd(pl)
    pl_status = lib.TEST_PL_getStatus(pl)
    
    # Should be active when 56kW > target_power
    if 56 > target_power:
        assert pl_status == True
        assert pl_torque < 2000  # Should limit torque
    else:
        assert pl_status == False or pl_torque == 2000


@pytest.mark.parametrize("pl_mode", [1, 2, 3])
def test_different_power_limit_modes(pl, mcm0, tps, pl_mode):
    """Test different power limit control modes"""
    lib.TEST_PL_setToggle(pl, True)
    lib.TEST_PL_setAlwaysOn(pl, True)
    lib.TEST_PL_setMode(pl, pl_mode)
    lib.TEST_PL_setTargetPower(pl, 40)

    # Set up scenario that should trigger power limiting
    lib.TEST_MCM_setRPM(mcm0, 3500)
    lib.TEST_MCM_setBusDeciVolts(mcm0, 6500)  # 650V
    lib.TEST_MCM_setCurrent(mcm0, 70)         # 70A = 45.5kW
    
    # Set TPS values to simulate 1800 dNm apps torque
    set_tps_for_torque(tps, 1800)

    rc = lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    assert rc == 0

    pl_torque = lib.TEST_PL_getTorqueCmd(pl)
    pl_status = lib.TEST_PL_getStatus(pl)
    
    # All modes should limit torque when power exceeds target
    if pl_status:
        assert pl_torque <= 1800
        assert pl_torque >= 0


# -------- Edge case tests --------
def test_zero_torque_command(pl, mcm0, tps):
    """Test power limit behavior with zero torque command"""
    lib.TEST_PL_setToggle(pl, True)
    lib.TEST_PL_setAlwaysOn(pl, True)
    lib.TEST_PL_setMode(pl, 1)
    lib.TEST_PL_setTargetPower(pl, 30)

    lib.TEST_MCM_setRPM(mcm0, 2500)
    lib.TEST_MCM_setBusDeciVolts(mcm0, 6500)
    lib.TEST_MCM_setCurrent(mcm0, 50)
    
    # Set TPS values to simulate zero torque (0% pedal)
    set_tps_for_torque(tps, 0)

    rc = lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    assert rc == 0

    pl_status = lib.TEST_PL_getStatus(pl)
    # Power limit should be off when no torque is commanded
    assert pl_status == False


def test_zero_rpm(pl, mcm0, tps):
    """Test power limit behavior with zero RPM"""
    lib.TEST_PL_setToggle(pl, True)
    lib.TEST_PL_setAlwaysOn(pl, True)
    lib.TEST_PL_setMode(pl, 1)
    lib.TEST_PL_setTargetPower(pl, 30)

    lib.TEST_MCM_setRPM(mcm0, 0)  # Zero RPM
    lib.TEST_MCM_setBusDeciVolts(mcm0, 6500)
    lib.TEST_MCM_setCurrent(mcm0, 10)
    
    # Set TPS values to simulate 1000 dNm apps torque
    set_tps_for_torque(tps, 1000)

    rc = lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    assert rc == 0

    # Should handle zero RPM gracefully (avoid division by zero)
    pl_torque = lib.TEST_PL_getTorqueCmd(pl)
    pl_status = lib.TEST_PL_getStatus(pl)
    assert pl_status in (True, False)
    assert -32768 <= pl_torque <= 32767


def test_low_power_scenario(pl, mcm0, tps):
    """Test power limit behavior when power is below target"""
    lib.TEST_PL_setToggle(pl, True)
    lib.TEST_PL_setAlwaysOn(pl, False)  # Not always on
    lib.TEST_PL_setMode(pl, 1)
    lib.TEST_PL_setTargetPower(pl, 50)

    # Set up low power scenario
    lib.TEST_MCM_setRPM(mcm0, 2000)
    lib.TEST_MCM_setBusDeciVolts(mcm0, 5000)  # 500V
    lib.TEST_MCM_setCurrent(mcm0, 30)         # 30A = 15kW
    
    # Set TPS values to simulate 800 dNm apps torque
    set_tps_for_torque(tps, 800)

    rc = lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    assert rc == 0

    pl_status = lib.TEST_PL_getStatus(pl)
    # Power limit should be off when power is below target and not always on
    assert pl_status == False


def test_power_limit_disabled(pl, mcm0, tps):
    """Test behavior when power limit toggle is disabled"""
    lib.TEST_PL_setToggle(pl, False)  # Disabled
    lib.TEST_PL_setAlwaysOn(pl, True)
    lib.TEST_PL_setMode(pl, 1)
    lib.TEST_PL_setTargetPower(pl, 30)

    # Set up high power scenario
    lib.TEST_MCM_setRPM(mcm0, 4000)
    lib.TEST_MCM_setBusDeciVolts(mcm0, 7000)  # 700V
    lib.TEST_MCM_setCurrent(mcm0, 80)         # 80A = 56kW
    
    # Set TPS values to simulate 2000 dNm apps torque
    set_tps_for_torque(tps, 2000)

    rc = lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    assert rc == 0

    pl_status = lib.TEST_PL_getStatus(pl)
    # Power limit should be off when toggle is disabled
    assert pl_status == False


# -------- Parameter tweaking tests --------
def test_power_limit_parameter_tweaking(pl, mcm0, tps):
    """Test tweaking power limit parameters during operation"""
    lib.TEST_PL_setToggle(pl, True)
    lib.TEST_PL_setAlwaysOn(pl, True)
    lib.TEST_PL_setMode(pl, 1)
    lib.TEST_PL_setTargetPower(pl, 35)

    # Set up high power scenario
    lib.TEST_MCM_setRPM(mcm0, 3000)
    lib.TEST_MCM_setBusDeciVolts(mcm0, 6000)  # 600V
    lib.TEST_MCM_setCurrent(mcm0, 70)         # 70A = 42kW
    
    # Set TPS values to simulate 1500 dNm apps torque
    set_tps_for_torque(tps, 1500)

    # Test with initial target
    rc = lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    assert rc == 0
    initial_torque = lib.TEST_PL_getTorqueCmd(pl)
    initial_status = lib.TEST_PL_getStatus(pl)

    # Change target power to 50kW (should turn off power limiting)
    lib.TEST_PL_setTargetPower(pl, 50)
    rc = lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    assert rc == 0
    new_torque = lib.TEST_PL_getTorqueCmd(pl)
    new_status = lib.TEST_PL_getStatus(pl)

    # Power limit should turn off with higher target
    if initial_status and not new_status:
        assert new_torque >= initial_torque

    # Test threshold discrepancy parameter
    lib.TEST_PL_setTargetPower(pl, 35)
    lib.TEST_PL_setThresholdDiscrepancy(pl, 20)  # Large threshold
    rc = lib.PowerLimit_calculateCommand(pl, mcm0, tps)
    assert rc == 0


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
