import time

from sre_test.hil.core import (
    CANInterface,
    DBCLoader,
    HILParamInjector,
    EfficiencyController,
    EfficiencyMonitor,
)

with CANInterface() as can:
    dbc = DBCLoader("/Users/dangeb/Documents/College/SJSU/Spartan Racing/VCU/dbcs/10.22.25_SRE_Main.dbc")
    injector = HILParamInjector(can)

    eff = EfficiencyController(injector)
    eff_monitor = EfficiencyMonitor(dbc, can)

    # Enable efficiency
    eff.set_toggle(1)

    # Set lap energy parameters
    eff.set_energy_budget_kwh(0.30)
    #eff.set_carry_over_energy_kwh(0.05)
    for i in range(2000):
        eff.set_total_lap_distance_km(i * 0.001)
        eff.send_all()

        # status = eff_monitor.recv(timeout=0.01)
        status = eff_monitor.wait_for_status(timeout_s=0.05, poll_timeout_s=0.01)
        if status:
            print(f"Efficiency Lap Counter: {status.lap_counter}")
            print(f"Efficiency Total Lap Distance: {status.totalLapDistance_km:.2f} km")
        time.sleep(0.01)
    print("Efficiency parameters injected successfully.")

    # Monitor VCU response
    # for _ in range(100):
    #     status = eff_monitor.recv(timeout=0.01)
    #     if status:
    #         print(f"Efficiency Lap Counter: {status.lap_counter}")
    #     time.sleep(0.01)