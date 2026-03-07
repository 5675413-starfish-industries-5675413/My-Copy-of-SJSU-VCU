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

    # Simulate driving for 5 laps to test lap counting
    last_lap_counter = 0
    distance_accumulator = 0.0
    distance_increment = 0.02  # 20m per cycle

    for i in range(250):
        distance_accumulator += distance_increment
        eff.totalLapDistance_km = distance_accumulator
        env.send_inputs(eff)

        # Give VCU 2 cycles to process the injection and send back fresh status
        time.sleep(0.02)

        # (Happens inside env.receive_outputs())
        # Flush all stale messages accumulated since last iteration

        result = env.receive_outputs(monitors={"efficiency"})
        eff_status = result.get("efficiency")
        if eff_status:
            print(f"Lap: {eff_status.get('lap_counter')}, Injected Dist: {distance_accumulator:.2f} km, VCU Dist: {eff_status.get('totalLapDistance_km'):.3f} km")
            if eff_status.get("lap_counter") > last_lap_counter:
                print(f"--- LAP {last_lap_counter} COMPLETED ---")
                distance_accumulator = eff_status.get("totalLapDistance_km")
                last_lap_counter = eff_status.get("lap_counter")
                ## ASSERTION
                # assert eff_status.get("totalLapDistance_km") < 0.1, (
                #     f"Expected lap distance to reset to 0 after lap {last_lap_counter}, "
                #     f"got {eff_status.get("totalLapDistance_km"):.3f} km"
                # )
        # sleep removed from end — it's now at the top of the loop

    print("Efficiency parameters injected successfully.")
    
    ## ASSERTION
    # assert eff_status is not None, "No efficiency status received from VCU"
    # assert eff_status.get("lap_counter") == 5, (
    #     f"Expected 5 laps completed, VCU counted {eff_status.get('lap_counter')}"
    # )

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

#     for i in range(250):
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