"""
Pytest test to run main.c
=========================
Compiles main.c with SIL mock headers and runs it.
"""

import pytest
import subprocess
import os
from pathlib import Path
import json
import csv
import extract_struct_members
from multiprocessing import Pool, Manager
import re
from functools import partial

# Get paths
script_dir = Path(__file__).parent  # dev/sil/build_main_sil
build_dir = script_dir  # We're already in the build directory
sil_dir = script_dir.parent  # dev/sil
dev_dir = sil_dir.parent  # dev
root_dir = dev_dir.parent  # VCU root
inc_dir = root_dir / 'inc'
sil_inc_dir = sil_dir / 'inc'
main_exe = build_dir / 'main.exe'
# JSON file path for PL_config.json
file_path = script_dir / 'json_files/PL_config.json'

def test_PL():
    extract_struct_members.main()
    
    # Map struct names to their config filenames
    struct_config_files = {
        'PowerLimit': 'PL_config.json',
        'MotorController': 'MCM_config.json',
        'TorqueEncoder': 'TorqueEncoder_config.json',
    }
    
    apply_config_to_structs(struct_config_files)
    test_run_main()
    
    # do something with the output later

def test_Efficiency():
    extract_struct_members.main()
    
    # Map struct names to their config filenames
    struct_config_files = {
        'PowerLimit': 'PL_config.json',
        'MotorController': 'MCM_config.json',
        'TorqueEncoder': 'TorqueEncoder_config.json',
    }
    
    apply_config_to_structs(struct_config_files)
    test_run_main()
    
    # do something with the output later

def test_Efficiency_CSV(timeout_seconds=0.5, save_progress_every=1000, batch_size=50):
    """
    Test that reads CSV data and runs main.c for each row in parallel.
    Updates MCM_config.json with values from each CSV row.
    Stores results in a new CSV file.
    
    Args:
        timeout_seconds: Timeout for each main.exe execution (default: 1.0)
        save_progress_every: Save progress every N rows (default: 1000, set to None to disable)
        batch_size: Number of rows to process per execution (default: 1, try 10-50 for speedup)
    """
    extract_struct_members.main()
    
    # Paths
    csv_file = script_dir / 'csv_files' / 'eff_data.csv'
    output_csv_file = script_dir / 'csv_files' / 'eff_results.csv'
    json_files_dir = script_dir / 'json_files'
    mcm_config_path = json_files_dir / 'MCM_config.json'
    
    if not csv_file.exists():
        pytest.skip(f"CSV file not found: {csv_file}")
    
    # Read CSV file
    with open(csv_file, 'r', encoding='utf-8') as f:
        csv_reader = csv.DictReader(f)
        # Skip the units row (second row) and any empty rows
        rows = []
        for row in csv_reader:
            time_value = row.get('Time', '').strip().strip('"')
            # Skip empty rows
            if not time_value:
                continue
            # Skip units row (contains "s" instead of a number)
            try:
                float(time_value)  # If this succeeds, it's a valid time value
                rows.append(row)
            except ValueError:
                # Not a number, skip this row (likely the units row)
                continue
    
    if not rows:
        pytest.skip("No data rows found in CSV file")
    
    # Map struct names to their config filenames
    struct_config_files = {
        'PowerLimit': 'PL_config.json',
        'MotorController': 'MCM_config.json',
        'TorqueEncoder': 'TorqueEncoder_config.json',
    }
    
    # Compile once before processing (more efficient)
    test_run_main(compile_only=True)
    
    # Determine number of parallel workers
    num_workers = os.cpu_count() or 4
    
    # Batch rows for processing
    if batch_size > 1:
        # Group rows into batches
        batched_rows = []
        for i in range(0, len(rows), batch_size):
            batch = rows[i:i+batch_size]
            batched_rows.append((i, batch))  # (start_index, batch_rows)
        print(f"Grouped {len(rows)} rows into {len(batched_rows)} batches of ~{batch_size} rows")
    else:
        # Process individually
        batched_rows = [(i, [row]) for i, row in enumerate(rows)]
    
    # Prepare worker function with shared parameters (no file lock needed with temp files)
    worker_func = partial(
        process_csv_batch_worker,
        script_dir=str(script_dir),
        json_files_dir=str(json_files_dir),
        mcm_config_path=str(mcm_config_path),
        struct_config_files=struct_config_files,
        timeout_seconds=timeout_seconds,
        batch_size=batch_size
    )
    
    print(f"Processing {len(rows)} rows with {num_workers} parallel workers...")
    print(f"Timeout per execution: {timeout_seconds}s")
    print(f"Batch size: {batch_size} rows per execution")
    if save_progress_every:
        print(f"Progress will be saved every {save_progress_every} rows")
    
    # Define output columns
    fieldnames = list(rows[0].keys()) + [
        'time_in_straight',
        'time_in_corners',
        'lap_counter',
        'pl_target',
        'energy_consumption_per_lap',
        'energy_budget_per_lap',
        'carry_over_energy'
    ]
    
    # Initialize output CSV file
    output_csv_file.parent.mkdir(parents=True, exist_ok=True)
    results_written = 0
    
    # Process batches in parallel
    with Pool(processes=num_workers) as pool:
        results_iter = pool.imap(worker_func, batched_rows)
        
        # Open file for writing
        with open(output_csv_file, 'w', newline='', encoding='utf-8') as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            
            # Process results as they come in
            for batch_results in results_iter:
                if batch_results:
                    for result in batch_results:
                        if result is not None:
                            writer.writerow(result)
                            results_written += 1
                            
                            # Save progress periodically
                            if save_progress_every and results_written % save_progress_every == 0:
                                f.flush()
                                print(f"Progress: {results_written}/{len(rows)} rows processed ({100*results_written/len(rows):.1f}%)")
    
    print(f"Completed! {results_written} results written to {output_csv_file}")

"""
Helper functions
=========================
"""

def process_csv_batch_worker(batch_data, script_dir, json_files_dir, mcm_config_path, struct_config_files, timeout_seconds=1.0, batch_size=1):
    """
    Worker function to process a batch of CSV rows.
    Uses per-worker temporary files to avoid file locking.
    
    Args:
        batch_data: Tuple of (start_index, list_of_csv_row_dicts)
        script_dir, json_files_dir, mcm_config_path, struct_config_files: Paths and config (as strings)
        timeout_seconds: Timeout per execution
        batch_size: Number of rows per batch
    
    Returns:
        List of dictionaries with input row data + output efficiency data
    """
    from pathlib import Path
    import shutil
    import tempfile
    
    start_idx, batch_rows = batch_data
    script_dir = Path(script_dir)
    json_files_dir = Path(json_files_dir)
    mcm_config_path = Path(mcm_config_path)
    worker_id = os.getpid()
    
    results = []
    
    try:
        # Create worker-specific temporary directory to avoid conflicts
        with tempfile.TemporaryDirectory(prefix=f'sil_worker_{worker_id}_') as temp_dir:
            temp_dir = Path(temp_dir)
            
            # Copy base files to temp directory
            temp_json_dir = temp_dir / 'json_files'
            temp_json_dir.mkdir()
            
            # Copy base MCM config
            temp_mcm_config = temp_json_dir / 'MCM_config.json'
            shutil.copy(mcm_config_path, temp_mcm_config)
            
            # Copy other config files
            for struct_name, config_file in struct_config_files.items():
                if struct_name != 'MotorController':
                    shutil.copy(json_files_dir / config_file, temp_json_dir / config_file)
            
            # Copy struct_members_output.json
            temp_struct_members = temp_json_dir / 'struct_members_output.json'
            shutil.copy(json_files_dir / 'struct_members_output.json', temp_struct_members)
            
            # Process each row in the batch
            for row_idx, csv_row in enumerate(batch_rows):
                try:
                    # Update MCM config with values from CSV row
                    update_mcm_config_from_csv_row(temp_mcm_config, csv_row)
                    
                    # Apply configs to structs (updates struct_members_output.json in temp dir)
                    apply_config_to_structs(struct_config_files, temp_json_dir)
                    
                    # Change working directory so main.exe reads from temp/json_files/
                    original_cwd = os.getcwd()
                    try:
                        os.chdir(temp_dir)
                        # Run main executable and capture output
                        output = run_main_executable_capture(script_dir, timeout_seconds)
                    finally:
                        os.chdir(original_cwd)
                    
                    # Parse output to extract efficiency data
                    efficiency_data = parse_efficiency_output(output)
                    
                    # Combine input row data with output efficiency data
                    result = dict(csv_row)
                    result.update(efficiency_data)
                    results.append(result)
                    
                except Exception as e:
                    print(f"Error processing row {start_idx + row_idx}: {e}")
                    results.append(None)
        
        return results
        
    except Exception as e:
        print(f"Error processing batch starting at row {start_idx}: {e}")
        import traceback
        traceback.print_exc()
        return [None] * len(batch_rows)

def process_csv_row_worker(row_data, script_dir, json_files_dir, mcm_config_path, struct_config_files, file_lock, timeout_seconds=2.0):
    """
    Worker function to process a single CSV row.
    This runs in a separate process, so it needs its own copies of paths.
    
    Args:
        row_data: Tuple of (row_index, csv_row_dict)
        script_dir, json_files_dir, mcm_config_path, struct_config_files: Paths and config (as strings)
        file_lock: Multiprocessing lock for file operations
    
    Returns:
        Dictionary with input row data + output efficiency data, or None if failed
    """
    from pathlib import Path
    import shutil
    
    row_idx, csv_row = row_data
    script_dir = Path(script_dir)
    json_files_dir = Path(json_files_dir)
    mcm_config_path = Path(mcm_config_path)
    
    try:
        # Use lock to serialize file operations (main.c reads from fixed path)
        with file_lock:
            # Update MCM_config.json with values from CSV row
            update_mcm_config_from_csv_row(mcm_config_path, csv_row)
            
            # Apply configs to structs (updates struct_members_output.json)
            apply_config_to_structs(struct_config_files, json_files_dir)
        
        # Run main executable and capture output (can run in parallel)
        output = run_main_executable_capture(script_dir, timeout_seconds)
        
        # Parse output to extract efficiency data
        efficiency_data = parse_efficiency_output(output)
        
        # Combine input row data with output efficiency data
        result = dict(csv_row)
        result.update(efficiency_data)
        
        return result
        
    except Exception as e:
        print(f"Error processing row {row_idx}: {e}")
        import traceback
        traceback.print_exc()
        return None

def parse_efficiency_output(output_text):
    """
    Parse efficiency data from main.exe output.
    
    Expected output format:
    time in straight: <value>
    time in corners: <value>
    lap counter: <value>
    PL Target: <value>
    Energy consumption per lap: <value>
    Energy budget per lap: <value>
    Carry over energy: <value>
    
    Returns:
        Dictionary with parsed values
    """
    result = {
        'time_in_straight': None,
        'time_in_corners': None,
        'lap_counter': None,
        'pl_target': None,
        'energy_consumption_per_lap': None,
        'energy_budget_per_lap': None,
        'carry_over_energy': None
    }
    
    # Parse using regex
    patterns = {
        'time_in_straight': r'time in straight:\s*([\d.]+)',
        'time_in_corners': r'time in corners:\s*([\d.]+)',
        'lap_counter': r'lap counter:\s*([\d.]+)',
        'pl_target': r'PL Target:\s*([\d.]+)',
        'energy_consumption_per_lap': r'Energy consumption per lap:\s*([\d.]+)',
        'energy_budget_per_lap': r'Energy budget per lap:\s*([\d.]+)',
        'carry_over_energy': r'Carry over energy:\s*([\d.]+)'
    }
    
    for key, pattern in patterns.items():
        match = re.search(pattern, output_text)
        if match:
            try:
                result[key] = float(match.group(1))
            except ValueError:
                pass
    
    return result


def run_main_executable_capture(script_dir, timeout_seconds=1.0):
    """
    Run the already-compiled main.exe executable and capture output.
    Returns the combined stdout and stderr output as a string.
    
    Args:
        script_dir: Path to script directory (can be string or Path)
        timeout_seconds: Timeout for execution (default: 2.0)
    """
    import subprocess as sp
    from pathlib import Path
    
    script_dir = Path(script_dir)
    main_exe = script_dir / 'main.exe'
    build_dir = script_dir
    
    if not main_exe.exists():
        return ""
    
    try:
        process = sp.Popen(
            [str(main_exe.resolve())],
            cwd=str(build_dir.resolve()),
            stdout=sp.PIPE,
            stderr=sp.PIPE,
            text=True,
            bufsize=0
        )
        
        try:
            stdout, stderr = process.communicate(timeout=timeout_seconds)
        except sp.TimeoutExpired:
            process.kill()
            stdout, stderr = process.communicate()
        
        # Combine stdout and stderr
        return stdout + stderr
        
    except Exception as e:
        return f"Error: {e}"

def update_mcm_config_from_csv_row(mcm_config_path, csv_row):
    """
    Update MCM_config.json with values from a CSV row.
    
    Args:
        mcm_config_path: Path to MCM_config.json file
        csv_row: Dictionary with CSV row data (keys are column names)
    """
    # Read current MCM config
    with open(mcm_config_path, 'r') as f:
        mcm_config = json.load(f)
    
    # Map CSV column names to MCM_config.json parameter names
    # CSV columns: "MCM DC Bus Voltage", "MCM DC Bus Current", "MCM Motor Speed", "MCM Torque Command"
    # MCM_config parameters: "DC_Voltage", "DC_Current", "motorRPM", "commandedTorque"
    
    csv_to_mcm_mapping = {
        'MCM DC Bus Voltage': 'DC_Voltage',
        'MCM DC Bus Current': 'DC_Current',
        'MCM Motor Speed': 'motorRPM',
        'MCM Torque Command': 'commandedTorque',
    }
    
    # Update MCM config parameters from CSV row
    for csv_key, mcm_key in csv_to_mcm_mapping.items():
        if csv_key in csv_row and csv_row[csv_key]:
            try:
                # Remove quotes and convert to appropriate type
                value_str = csv_row[csv_key].strip().strip('"')
                if value_str:
                    # Try to convert to int first, then float if needed
                    try:
                        value = int(float(value_str))  # Convert via float first to handle "0.0" -> 0
                    except ValueError:
                        value = float(value_str)
                    
                    # Update the parameter in MCM config
                    if mcm_key in mcm_config['parameters']:
                        mcm_config['parameters'][mcm_key] = value
            except (ValueError, KeyError) as e:
                # Skip invalid values
                print(f"Warning: Could not convert {csv_key}='{csv_row.get(csv_key)}' to number: {e}")
    
    # Write updated MCM config back to file
    with open(mcm_config_path, 'w') as f:
        json.dump(mcm_config, f, indent=2)

def apply_config_to_structs(struct_config_files, json_files_dir=None):
    """
    Helper function to apply config file values to struct members.
    
    Args:
        struct_config_files: Dictionary mapping struct names to their config filenames
                            e.g., {'PowerLimit': 'PL_config.json', ...}
        json_files_dir: Optional path to json_files directory (defaults to script_dir / 'json_files')
    """
    if json_files_dir is None:
        json_files_dir = script_dir / 'json_files'
    else:
        json_files_dir = Path(json_files_dir)
    struct_members_path = json_files_dir / 'struct_members_output.json'
    
    with open(struct_members_path, 'r') as f:
        struct_members = json.load(f)
    
    # Load all config files dynamically
    configs = {}
    for struct_name, config_file in struct_config_files.items():
        with open(json_files_dir / config_file, 'r') as f:
            configs[struct_name] = json.load(f)
    
    # Copy config values into struct members dynamically
    for struct_name, config in configs.items():
        struct = next((s for s in struct_members if s['name'] == struct_name), None)
        if struct:
            struct_fields = struct['parameters']
            for field, value in config['parameters'].items():
                if field in struct_fields:
                    # Assign the value from config to struct_members_output.json
                    # This handles both simple values and nested structures (like "pid")
                    struct_fields[field] = value
                else:
                    print(f"Field {field} not found in struct {struct_name}")

    # Write the updated struct_members back to struct_members_output.json in json_files directory
    with open(struct_members_path, 'w') as f:
        json.dump(struct_members, f, indent=2)

def test_run_main(compile_only=False):
    """
    Compile main.c with SIL headers and optionally run it.
    
    Args:
        compile_only: If True, only compile and don't run the executable
    """
    # Note: Run pytest with -s flag to see output: pytest -s test_main.py::test_PL
    # Check if gcc is available
    try:
        subprocess.run(['gcc', '--version'], capture_output=True, timeout=5, check=True)
    except (FileNotFoundError, subprocess.TimeoutExpired, subprocess.CalledProcessError):
        pytest.skip("gcc not available")
    
    # Create build directory
    build_dir.mkdir(exist_ok=True)
    
    # List of all source files needed for main.c
    source_files = [
        dev_dir / 'main.c',
        dev_dir / 'initializations.c',
        dev_dir / 'sensors.c',
        dev_dir / 'canManager.c',
        dev_dir / 'motorController.c',
        dev_dir / 'instrumentCluster.c',
        dev_dir / 'readyToDriveSound.c',
        dev_dir / 'torqueEncoder.c',
        dev_dir / 'brakePressureSensor.c',
        dev_dir / 'wheelSpeeds.c',
        dev_dir / 'safety.c',
        dev_dir / 'sensorCalculations.c',
        dev_dir / 'serial.c',
        dev_dir / 'cooling.c',
        dev_dir / 'bms.c',
        dev_dir / 'LaunchControl.c',
        dev_dir / 'regen.c',
        dev_dir / 'drs.c',
        dev_dir / 'powerLimit.c',
        dev_dir / 'PID.c',
        dev_dir / 'efficiency.c',
        dev_dir / 'hashTable.c',
        dev_dir / 'avlTree.c',
        dev_dir / 'watchdog.c',
        dev_dir / 'mathFunctions.c',
        # SIL stub files
        sil_inc_dir / 'cstart_sil.c',  # Stub for _cstart function
        # Note: sensors_sil_stubs.c removed - Sensor variables are defined in initializations.c
        sil_inc_dir / 'IO_Driver_sil.c',  # Stub for IO_Driver functions
        sil_inc_dir / 'IO_RTC_sil.c',
        sil_inc_dir / 'IO_UART_sil.c',
        sil_inc_dir / 'IO_ADC_sil.c',
        sil_inc_dir / 'IO_CAN_sil.c',
        sil_inc_dir / 'IO_DIO_sil.c',
        sil_inc_dir / 'IO_PWM_sil.c',
        sil_inc_dir / 'IO_EEPROM_sil.c',
        sil_inc_dir / 'IO_POWER_sil.c',
        sil_inc_dir / 'IO_PWD_sil.c',
        sil_inc_dir / 'IO_WDTimer_sil.c',
        sil_inc_dir / 'DIAG_Functions_sil.c',
        # JSON parsing library and parser
        sil_inc_dir / 'cJSON.c',  # cJSON library for JSON parsing
        script_dir / 'parse_values.c',  # JSON parser for struct initialization
    ]
    
    # Filter out files that don't exist and convert to resolved absolute paths
    existing_sources = [str(f.resolve()) for f in source_files if f.exists()]
    
    if not existing_sources:
        pytest.skip("No source files found")
    
    # Compiler flags
    # Suppress warnings that are expected for embedded code compiled for PC
    cflags = [
        '-O2', 
        '-Wall', 
        '-Wno-unused-variable',
        '-Wno-main',  # Allow void main() for embedded code
        '-Wno-pointer-sign',  # Suppress char* vs ubyte1* warnings
        '-Wno-incompatible-pointer-types',  # Suppress pointer type warnings
        '-Wno-format-overflow',  # Suppress format overflow warnings
        '-Wno-array-bounds',  # Suppress array bounds warnings
        '-Wno-unused-function',  # Suppress unused function warnings
        '-Wno-implicit-function-declaration',  # Suppress implicit function declaration warnings
        '-Wno-builtin-declaration-mismatch',  # Suppress builtin declaration mismatch
        # Define RTS_TTC_FLASH_DATE constants (required by main.c)
        '-DRTS_TTC_FLASH_DATE_YEAR=2025',
        '-DRTS_TTC_FLASH_DATE_MONTH=1',
        '-DRTS_TTC_FLASH_DATE_DAY=1',
        '-DRTS_TTC_FLASH_DATE_HOUR=0',
        '-DRTS_TTC_FLASH_DATE_MINUTE=0',
        # Define SIL_BUILD to enable SIL-specific code
        '-DSIL_BUILD',
    ]
    # Math library - must come after source files for linking
    ldflags = ['-lm']
    # Include SIL headers first so they override real headers
    # Use resolve() for normalized absolute paths
    includes = [
        f'-I{str(sil_inc_dir.resolve())}',  # SIL headers take precedence (checked first), also contains cJSON.h
        f'-I{str(script_dir.resolve())}',  # Contains parse_values.h
        f'-I{str(inc_dir.resolve())}',
        f'-I{str(dev_dir.resolve())}',
    ]
    
    # Debug: Print include paths
    print("Include paths:", includes)
    print("Number of source files:", len(existing_sources))
    
    # Compile main.c with all dependencies
    try:
        compile_cmd = ['gcc'] + cflags + includes + existing_sources + ['-o', str(main_exe.resolve())] + ldflags
        result = subprocess.run(
            compile_cmd,
            cwd=str(build_dir.resolve()),
            capture_output=True,
            text=True,
            timeout=120
        )
        
        if result.returncode != 0:
            # Print the command for debugging
            print(f"Compilation command: {' '.join(compile_cmd[:10])}...")  # Print first 10 args
            pytest.fail(f"Compilation failed:\nSTDOUT: {result.stdout}\nSTDERR: {result.stderr}")
            
    except subprocess.TimeoutExpired:
        pytest.fail("Compilation timed out")
    except Exception as e:
        pytest.fail(f"Compilation error: {e}")
    
    # Check if executable was created
    if not main_exe.exists():
        pytest.fail("Compilation completed but executable not found")
    
    # If compile_only is True, return here without running
    if compile_only:
        return
    
    # Run main.c with timeout (it has an infinite loop)
    # Use Popen to capture output even when timeout occurs
    import subprocess as sp
    import sys
    try:
        exe_path = str(main_exe.resolve())
        sys.stderr.write(f"\nRunning executable: {exe_path}\n")
        sys.stderr.flush()
        
        # Verify executable exists
        if not main_exe.exists():
            pytest.fail(f"Executable not found: {exe_path}")
        
        process = sp.Popen(
            [exe_path],
            cwd=str(build_dir.resolve()),
            stdout=sp.PIPE,
            stderr=sp.PIPE,
            text=True,
            bufsize=0  # Unbuffered
        )
        
        try:
            stdout, stderr = process.communicate(timeout=2.0)
            sys.stderr.write("\nProgram completed (unexpected - should run forever)\n")
        except sp.TimeoutExpired:
            # Kill the process and get any output before timeout
            sys.stderr.write("\nProgram timed out (expected - main.c has infinite loop)\n")
            process.kill()
            stdout, stderr = process.communicate()
        
        # Always print output, even if empty - use sys.stderr so pytest shows it
        sys.stderr.write(f"\n=== Program Output (STDOUT) ===\n")
        if stdout:
            sys.stderr.write(stdout)
        else:
            sys.stderr.write("(empty)\n")
            
        sys.stderr.write(f"\n=== Program Output (STDERR) ===\n")
        if stderr:
            sys.stderr.write(stderr)
        else:
            sys.stderr.write("(empty)\n")
        sys.stderr.flush()
            
    except Exception as e:
        sys.stderr.write(f"\nException occurred: {e}\n")
        sys.stderr.flush()
        pytest.fail(f"Failed to run main.c: {e}")

def run_main_executable():
    """
    Run the already-compiled main.exe executable.
    Assumes the executable has already been compiled.
    """
    import subprocess as sp
    import sys
    
    # Verify executable exists
    if not main_exe.exists():
        pytest.fail(f"Executable not found: {main_exe}. Run test_run_main() first to compile.")
    
    try:
        exe_path = str(main_exe.resolve())
        
        process = sp.Popen(
            [exe_path],
            cwd=str(build_dir.resolve()),
            stdout=sp.PIPE,
            stderr=sp.PIPE,
            text=True,
            bufsize=0  # Unbuffered
        )
        
        try:
            stdout, stderr = process.communicate(timeout=2.0)
            sys.stderr.write("\nProgram completed (unexpected - should run forever)\n")
        except sp.TimeoutExpired:
            # Kill the process and get any output before timeout
            sys.stderr.write("\nProgram timed out (expected - main.c has infinite loop)\n")
            process.kill()
            stdout, stderr = process.communicate()
        
        # Always print output, even if empty - use sys.stderr so pytest shows it
        sys.stderr.write(f"\n=== Program Output (STDOUT) ===\n")
        if stdout:
            sys.stderr.write(stdout)
        else:
            sys.stderr.write("(empty)\n")
            
        sys.stderr.write(f"\n=== Program Output (STDERR) ===\n")
        if stderr:
            sys.stderr.write(stderr)
        else:
            sys.stderr.write("(empty)\n")
        sys.stderr.flush()
            
    except Exception as e:
        sys.stderr.write(f"\nException occurred: {e}\n")
        sys.stderr.flush()
        pytest.fail(f"Failed to run main.c: {e}")