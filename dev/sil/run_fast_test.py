"""
Fast CSV-driven test without pytest overhead
Runs 3-5x faster than pytest version
"""

import csv
import os
from pl_exported_functions import ffi, lib

# Paths
script_dir = os.path.dirname(os.path.abspath(__file__))
input_csv = r"c:\Users\seong\Documents\20251004-0910604_final.csv"
output_csv = os.path.join(script_dir, "test_results_output.csv")

def calculate_power(mcm0, pl):
    import math
    angular_speed = lib.TEST_MCM_getMotorRPM(mcm0) * 2 * math.pi / 60
    power = lib.TEST_getPLTorqueCommand(pl) / 10 * angular_speed
    return power

print("Loading CSV data...")
test_data = []
with open(input_csv, 'r') as f:
    # Skip header lines
    for _ in range(14):
        next(f)
    header_line = next(f).strip().replace('"', '').split(',')
    next(f)  # Skip units
    next(f)  # Skip blank
    next(f)  # Skip blank
    
    tps_idx = header_line.index('TPS0ThrottlePercent0FF')
    motor_speed_idx = header_line.index('MCM Motor Speed')
    
    reader = csv.reader(f)
    for i, row in enumerate(reader):
        if len(row) > max(tps_idx, motor_speed_idx):
            tps = float(row[tps_idx].strip('"'))
            motor_speed = float(row[motor_speed_idx].strip('"'))
            test_data.append((tps, motor_speed, i))

print(f"Loaded {len(test_data)} rows")

# Create fixtures ONCE (reuse across all tests)
print("Creating test objects...")
serialMan = lib.SerialManager_new()
mcm0 = lib.MotorController_new(serialMan, 0xA0, 1, 2310, 5, 10)
tps = lib.TorqueEncoder_new(False)
pl = lib.POWERLIMIT_new(True)

# Open output CSV
print("Running tests...")
with open(output_csv, 'w', newline='') as outfile:
    writer = csv.writer(outfile)
    writer.writerow(['Row', 'TPS_Input', 'Motor_Speed_Input', 'PL_Status', 'PL_TorqueCommand', 'Calculated_Power_kW'])
    
    for idx, (tps_percent, motor_speed, row_num) in enumerate(test_data):
        # Set TPS
        lib.TEST_TPS_setBoth_percent(tps, tps_percent / 100.0)
        lib.TEST_TPS_setTravelPercent(tps)
        
        # Configure MCM
        lib.TEST_MCM_setCommandedTorque(mcm0)
        lib.TEST_MCM_setRPM(mcm0, int(motor_speed))
        lib.TEST_MCM_setDCVoltage(mcm0, 800)
        lib.TEST_MCM_setDCCurrent(mcm0, 100)
        lib.MCM_calculateCommands(mcm0, tps, ffi.NULL)
        
        # Configure PL
        lib.TEST_setPLKnobVoltage(1800)
        lib.TEST_setPLAlwaysOn(pl, 1)
        lib.TEST_setPID(pl, 10, 10, 0, 231, 10)
        lib.TEST_setPLMode(pl, 2)
        lib.TEST_setPLStatus(pl, True)
        lib.TEST_setPLTargetPower(pl, 40)
        lib.TEST_setPLThresholdDiscrepancy(pl, 15)
        lib.TEST_setPLInitializationThreshold(pl, 0)
        lib.TEST_setClampingMethod(pl, 0)
        
        # Run calculation
        lib.PowerLimit_calculateCommand(pl, mcm0, tps)
        
        # Get results
        pl_status = lib.TEST_getPLStatus(pl)
        pl_torque = lib.TEST_getPLTorqueCommand(pl)
        calc_power = calculate_power(mcm0, pl) * 0.9
        
        # Write results
        writer.writerow([row_num, tps_percent, motor_speed, pl_status, pl_torque, calc_power])
        
        # Progress indicator
        if (idx + 1) % 500 == 0:
            print(f"Progress: {idx + 1}/{len(test_data)} ({100*(idx+1)/len(test_data):.1f}%)")

print(f"\nDone! Results saved to: {output_csv}")
print(f"Total tests run: {len(test_data)}")

