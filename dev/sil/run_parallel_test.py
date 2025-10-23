"""
Parallel CSV-driven test using multiprocessing
Each process gets its own copy of the C library
"""

import csv
import os
import multiprocessing as mp
from pl_exported_functions import ffi, lib
import math

def calculate_power(mcm0, pl):
    angular_speed = lib.TEST_MCM_getMotorRPM(mcm0) * 2 * math.pi / 60
    power = lib.TEST_getPLTorqueCommand(pl) / 10 * angular_speed
    return power

def process_chunk(chunk_data, chunk_id):
    """Process a chunk of data in a separate process"""
    print(f"Process {chunk_id}: Starting {len(chunk_data)} tests...")
    
    # Each process creates its own objects

    
    results = []
    
    for idx, (tps_percent, motor_speed, row_num) in enumerate(chunk_data):
        serialMan = lib.SerialManager_new()
        mcm0 = lib.MotorController_new(serialMan, 0xA0, 1, (int)(2310*.0984), 5, 10)
        tps = lib.TorqueEncoder_new(False)
        pl = lib.POWERLIMIT_new(True)
        
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
        lib.TEST_setPID(pl, 20, 10, 0, 231, 10)
        lib.TEST_setPLMode(pl, 1)
        lib.TEST_setPLStatus(pl, False)
        lib.TEST_setPLTorqueCommand(pl, 0)
        lib.TEST_setPLTargetPower(pl, 40)
        lib.TEST_setPLThresholdDiscrepancy(pl, 15)
        lib.TEST_setPLInitializationThreshold(pl, 0)
        lib.TEST_setClampingMethod(pl, 6)
        
        # Run calculation
        lib.PowerLimit_calculateCommand(pl, mcm0, tps)
        
        # Get results
        pl_status = lib.TEST_getPLStatus(pl)
        pl_torque = (int)(lib.TEST_getPLTorqueCommand(pl)/10 * 0.984) #convert to deciNewton-meters and apply mechanical torque loss
        calc_power = calculate_power(mcm0, pl)
        
        results.append([int(row_num), tps_percent, int(motor_speed), pl_status, int(pl_torque), int(calc_power)])
        
        if (idx + 1) % 100 == 0:
            print(f"Process {chunk_id}: {idx + 1}/{len(chunk_data)}")
    
    print(f"Process {chunk_id}: Completed!")
    return results

def main():
    # Paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    input_csv = r"i2pro_data2.csv"
    output_csv = os.path.join(script_dir, "output/test_results_output.csv")
    
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
    
    # Split data into chunks for parallel processing
    num_processes = mp.cpu_count()
    chunk_size = len(test_data) // num_processes
    chunks = [test_data[i:i + chunk_size] for i in range(0, len(test_data), chunk_size)]
    
    print(f"Using {num_processes} processes, {len(chunks)} chunks")
    
    # Run parallel processing
    with mp.Pool(processes=num_processes) as pool:
        results = pool.starmap(process_chunk, [(chunk, i) for i, chunk in enumerate(chunks)])
    
    # Flatten results and write to CSV
    print("Writing results...")
    all_results = []
    for chunk_results in results:
        all_results.extend(chunk_results)
    
    # Sort by row number to maintain order
    all_results.sort(key=lambda x: x[0])
    
    with open(output_csv, 'w', newline='') as outfile:
        writer = csv.writer(outfile)
        writer.writerow(['Row', 'TPS_Input', 'Motor_Speed_Input', 'PL_Status', 'PL_TorqueCommand', 'Calculated_Power_kW'])
        writer.writerows(all_results)
    
    print(f"\nDone! Results saved to: {output_csv}")
    print(f"Total tests run: {len(all_results)}")

if __name__ == "__main__":
    main()

