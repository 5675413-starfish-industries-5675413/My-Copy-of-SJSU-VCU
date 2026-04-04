"""
Efficiency Algorithm Test Script

Usage:
    pytest sre_test/tests/test_efficiency.py --backend sil
    pytest sre_test/tests/test_efficiency.py --backend hil

    TBD, will turn to:
    sre test efficiency --backend sil
    sre test efficiency --backend hil
"""

from sre_test.core.config import DynamicConfig
import time

def test_lap_counting(env):
    """Simulates driving for 5 laps to test lap counting and distance reset behavior."""
    eff = DynamicConfig("Efficiency")
    eff.efficiencyToggle = 1
    eff.energyBudget_kWh = 0.30

    # Drive lap counting from MCM ground-speed calculation in firmware.
    mcm = DynamicConfig("MotorController")
    mcm.motorRPM = 300000000
    target_laps = 5
    max_cycles = 20
    eff_status = None

    for i in range(max_cycles):
        env.send_inputs(eff, mcm)
        time.sleep(0.02)

        result = env.receive_outputs()
        eff_status = result.get("Efficiency") or result.get("efficiency")
        if eff_status:
            lap_counter = eff_status.get("lapCounter")
            if lap_counter is None:
                lap_counter = eff_status.get("lap_counter")

            print(f"Cycle {i}: lapCounter={lap_counter}")
            if isinstance(lap_counter, (int, float)) and int(lap_counter) >= target_laps:
                break

    print("Efficiency parameters injected successfully.")
    
    ## ASSERTION
    assert eff_status is not None, "No efficiency status received from VCU"
    final_lap_counter = eff_status.get("lapCounter")
    if final_lap_counter is None:
        final_lap_counter = eff_status.get("lap_counter")
    assert isinstance(final_lap_counter, (int, float)) and int(final_lap_counter) >= target_laps, (
        f"Expected at least {target_laps} laps completed, VCU counted {final_lap_counter}"
    )

# import time

# from sre_test.hil.core import (
#     CANInterface,
#     DBCLoader,
#     HILParamInjector,
#     EfficiencyController,
#     EfficiencyMonitor,
# )

# with CANInterface() as can:
#     dbc = DBCLoader("/Users/dangeb/Documents/College/SJSU/Spartan Racing/VCU/dbcs/10.22.25_SRE_Main.dbc")
#     injector = HILParamInjector(can)

#     eff = EfficiencyController(injector)
#     eff_monitor = EfficiencyMonitor(dbc, can)

#     # Enable efficiency
#     eff.set_toggle(1)

#     # Set lap energy parameters
#     eff.set_energy_budget_kwh(0.30)
#     #eff.set_carry_over_energy_kwh(0.05)

#     # Simulate driving for 5 laps to test lap counting
#     last_lap_counter = 0
#     distance_accumulator = 0.0
#     distance_increment = 0.02  # 20m per cycle

#     for i in range(250):      # Run for ~5 laps (250 steps * 0.02km/step = 5km)
#         distance_accumulator += distance_increment
#         eff.set_total_lap_distance_km(distance_accumulator)
#         eff.send_all()

#         # Give VCU 2 cycles to process the injection and send back fresh status
#         time.sleep(0.02)

#         # Flush all stale messages accumulated since last iteration
#         while can.receive(timeout=0) is not None:
#             pass

#         status = eff_monitor.wait_for_status(timeout_s=0.05, poll_timeout_s=0.01)
#         if status:
#             print(f"Lap: {status.lap_counter}, Injected Dist: {distance_accumulator:.2f} km, VCU Dist: {status.totalLapDistance_km:.3f} km")
#             if status.lap_counter > last_lap_counter:
#                 print(f"--- LAP {last_lap_counter} COMPLETED ---")
#                 distance_accumulator = status.totalLapDistance_km
#                 last_lap_counter = status.lap_counter
#         # sleep removed from end — it's now at the top of the loop

#     print("Efficiency parameters injected successfully.")


    #### MOST RECENT TEST CODE - 8:05 PM ####
    # # Get initial status to set the lap counter
    # status = eff_monitor.wait_for_status(timeout_s=0.1)
    # if status:
    #     last_lap_counter = status.lap_counter

    # print("Starting efficiency test simulation...")
    # for i in range(250):  # Run for ~5 laps (250 steps * 0.02km/step = 5km)
    #     distance_accumulator += distance_increment
    #     eff.set_total_lap_distance_km(distance_accumulator)
    #     eff.send_all()

    #     status = eff_monitor.wait_for_status(timeout_s=0.05, poll_timeout_s=0.01)
    #     if status:
    #         print(f"Lap: {status.lap_counter}, Injected Dist: {distance_accumulator:.2f} km, VCU Dist: {status.totalLapDistance_km:.3f} km")
    #         if status.lap_counter > last_lap_counter:
    #             print(f"--- LAP {last_lap_counter} COMPLETED ---")
    #             ### ENABLE WHILE LOOP IF DISTANCE RESET TO 0 IS NOT WORKING AS EXPECTED ###
    #             # while status is not None and status.totalLapDistance_km > 0:
    #             #     status = eff_monitor.wait_for_status(timeout_s=0.05, poll_timeout_s=0.01)
    #             distance_accumulator = status.totalLapDistance_km  # Resync with VCU's distance after lap reset
    #             last_lap_counter = status.lap_counter
    #     time.sleep(0.01)
    # print("Efficiency parameters injected successfully.")
    
def test_efficiency_2(env):
    mcm_config = DynamicConfig("MotorController")
    mcm_config.DC_Voltage = 200
    mcm_config.DC_Current = 300

    eff_config = DynamicConfig("Efficiency")
    eff_config.efficiencyToggle = True

    env.send_inputs(mcm_config, eff_config)
    result = env.receive_outputs(timeout=2.0)
    
    eff_status = result.get("Efficiency")
    print(f"StraightTime is: {eff_status.get('straightTime_s')}")
    print(f"CornerTime is: {eff_status.get('cornerTime_s')}")
    print(f"CornerEnergy is: {eff_status.get('cornerEnergy_kWh')}")
    print(f"StraightEnergy is: {eff_status.get('straightEnergy_kWh')}")
    print(f"LapEnergy is: {eff_status.get('lapEnergy_kWh')}")
    print(f"LapDistance is: {eff_status.get('lapDistance_km')}")
    print(f"FinishedLap is: {eff_status.get('finishedLap')}")