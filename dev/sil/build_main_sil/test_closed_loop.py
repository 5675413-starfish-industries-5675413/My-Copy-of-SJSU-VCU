import test_main as test_main
import pytest
from pathlib import Path
import json
import csv
import threading
import queue
import subprocess as sp




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

def test_Efficiency_closed_loop(timeout_per_row=2.0, save_progress_every=100, sil_output_mode=0x01):
    """
    Closed-loop test system with two threads:
    - Thread 1: Runs main.c continuously
    - Thread 2: Reads CSV rows, sends JSON via stdin, receives stdout, writes results to CSV

    Args:
        timeout_per_row: Timeout for reading output per CSV row (seconds)
        save_progress_every: Save progress every N rows
        sil_output_mode: SIL output mode configuration (0x01=efficiency, 0x02=power_limit, 
                         0x04=motor_controller, 0x07=all). Default is 0x01 (efficiency only).
    """

    test_main.extract_struct_members.main()

    # Paths
    csv_file = script_dir / 'csv_files' / 'eff_data.csv'
    output_csv_file = script_dir / 'csv_files' / 'eff_results.csv'
    json_files_dir = script_dir / 'json_files'

    if not csv_file.exists():
        pytest.skip(f"CSV file not found: {csv_file}")

    # Read CSV file
    with open(csv_file, 'r', encoding='utf-8') as f:
        csv_reader = csv.DictReader(f)
        rows = []
        for row in csv_reader:
            time_value = row.get('Time', '').strip().strip('"')
            if not time_value:
                continue
            try:
                float(time_value)
                rows.append(row)
            except ValueError:
                continue

    if not rows:
        pytest.skip("No data rows found in CSV file")

    # Load struct members
    struct_members_path = json_files_dir / 'struct_members_output.json'
    with open(struct_members_path, 'r') as f:
        struct_members_base = json.load(f)

    # Load PowerLimit config from JSON file (set once, not per row)
    pl_config_path = json_files_dir / 'PL_config.json'
    pl_config_struct = None
    if pl_config_path.exists():
        with open(pl_config_path, 'r') as f:
            pl_config_data = json.load(f)
            if pl_config_data.get("name") == "PowerLimit":
                pl_config_struct = {
                    "name": "PowerLimit",
                    "parameters": pl_config_data.get("parameters", {})
                }

    # Compile once with specified output mode
    test_main.test_run_main(compile_only=True, sil_output_mode=sil_output_mode)

    # Output CSV columns
    fieldnames = [
        'MCM Motor Speed',
        'time_in_straight',
        'time_in_corners',
        'lap_counter',
        'pl_target',
        'energy_consumption_per_lap',
        'energy_budget_per_lap',
        'carry_over_energy',
        'total_lap_distance',
    ]

    output_csv_file.parent.mkdir(parents=True, exist_ok=True)

    # Shared state
    process_holder = []
    process_ready = threading.Event()
    stop_event = threading.Event()
    stdout_queue = queue.Queue()

    # Start of child process
    def run_main_process():
        import subprocess as sp

        process = sp.Popen(
            [str(main_exe.resolve())],
            cwd=str(build_dir.resolve()),
            stdin=sp.PIPE,
            stdout=sp.PIPE,
            stderr=None,
            text=True,
            bufsize=1,
        )

        process_holder.append(process)
        process_ready.set()

        try:
            while not stop_event.is_set():
                if process.poll() is not None:
                    break
                line = process.stdout.readline()
                if not line:
                    break
                line = line.strip()
                if not line:
                    continue
                try:
                    stdout_queue.put(json.loads(line))
                except json.JSONDecodeError:
                    pass
        except Exception as e:
            if not stop_event.is_set():
                print(f"Error reading stdout: {e}")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=2.0)
                except sp.TimeoutExpired:
                    process.kill()
                    process.wait()
    # End of child process
    
    process_thread = threading.Thread(target=run_main_process, daemon=True)
    process_thread.start()

    if not process_ready.wait(timeout=5.0):
        pytest.fail("Process failed to start")

    process = process_holder[0]
    
    # Send PowerLimit config once at startup (if available)
    if pl_config_struct:
        try:
            pl_init_json = json.dumps({"row_id": -1, "structs": [pl_config_struct]}, separators=(",", ":"))
            process.stdin.write(pl_init_json + '\n')
            process.stdin.flush()
            # Give it a moment to process the initial config
            import time
            time.sleep(0.1)
        except Exception as e:
            print(f"Warning: Failed to send PowerLimit config: {e}")

    # Map CSV columns to struct parameters
    CSV_TO_MCM_PARAMS = {
        "MCM DC Bus Voltage": "DC_Voltage",
        "MCM DC Bus Current": "DC_Current",
        "MCM Motor Speed": "motorRPM",
        "MCM Torque Command": "commandedTorque",
    }
    
    CSV_TO_TPS_PARAMS = {
        "TPS0ThrottlePercent0FF": "tps0_percent",
        "TPS1ThrottlePercent0FF": "tps1_percent",
    }
    
    def fast_parse(x):
        if x is None:
            return None
        s = str(x).strip().strip('"')
        if not s:
            return None
        try:
            f = float(s)
        except ValueError:
            return None
        return int(f) if f.is_integer() else f
    
    def generate_json_string(csv_row, row_id):
        structs = []
        
        # MotorController parameters
        mcm_params = {}
        for csv_key, param_key in CSV_TO_MCM_PARAMS.items():
            val = fast_parse(csv_row.get(csv_key))
            if val is not None:
                mcm_params[param_key] = val
        if mcm_params:
            structs.append({
                "name": "MotorController",
                "parameters": mcm_params
            })
        
        # TorqueEncoder parameters
        tps_params = {}
        for csv_key, param_key in CSV_TO_TPS_PARAMS.items():
            val = fast_parse(csv_row.get(csv_key))
            if val is not None:
                tps_params[param_key] = val
        if tps_params:
            structs.append({
                "name": "TorqueEncoder",
                "parameters": tps_params
            })

        return json.dumps({"row_id": row_id, "structs": structs}, separators=(",", ":"))
    
    results_written = 0
    with open(output_csv_file, 'w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

        for row_idx, csv_row in enumerate(rows):
            if stop_event.is_set() or process.poll() is not None:
                break

            try:
                process.stdin.write(generate_json_string(csv_row, row_idx) + '\n')
                process.stdin.flush()
            except Exception as e:
                print(f"Error sending JSON at row {row_idx}: {e}")
                continue

            efficiency_data = {}
            try:
                response = stdout_queue.get(timeout=timeout_per_row)
                efficiency_data = response.get('efficiency', {})
            except queue.Empty:
                print(f"Timeout waiting for response at row {row_idx}")

            motor_rpm = csv_row.get('MCM Motor Speed', '').strip().strip('"')
            try:
                motor_rpm_value = float(motor_rpm) if motor_rpm else None
            except ValueError:
                motor_rpm_value = None

            # Build result row with motor RPM from input CSV and efficiency data from C process
            result_row = {k: efficiency_data.get(k) for k in fieldnames}
            result_row['MCM Motor Speed'] = motor_rpm_value  # Override with value from input CSV
            writer.writerow(result_row)

            results_written += 1
            if save_progress_every and results_written % save_progress_every == 0:
                f.flush()
                print(f"Progress: {results_written}/{len(rows)}")

    stop_event.set()
    process.terminate()
    process_thread.join(timeout=2.0)

    print(f"Completed! {results_written} results written to {output_csv_file}")
