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

def test_Efficiency_closed_loop(timeout_per_row=2.0, save_progress_every=100):
    """
    Closed-loop test system with two threads:
    - Thread 1: Runs main.c continuously
    - Thread 2: Reads CSV rows, sends JSON via stdin, receives stdout, writes results to CSV

    Args:
        timeout_per_row: Timeout for reading output per CSV row (seconds)
        save_progress_every: Save progress every N rows
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

    # Load configs
    configs = {}
    struct_config_files = {
        'PowerLimit': 'PL_config.json',
        'MotorController': 'MCM_config.json',
        'TorqueEncoder': 'TorqueEncoder_config.json',
    }

    for struct_name, config_file in struct_config_files.items():
        with open(json_files_dir / config_file, 'r') as f:
            configs[struct_name] = json.load(f)

    # In-memory MCM config
    mcm_config = configs['MotorController'].copy()

    # Compile once
    test_main.test_run_main(compile_only=True)

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

    # Start of thread 1
    def run_main_process():
        import subprocess as sp

        process = sp.Popen(
            [str(main_exe.resolve())],
            cwd=str(build_dir.resolve()),
            stdin=sp.PIPE,
            stdout=sp.PIPE,
            stderr=sp.PIPE,
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
    # End of thread 1
    
    process_thread = threading.Thread(target=run_main_process, daemon=True)
    process_thread.start()

    if not process_ready.wait(timeout=5.0):
        pytest.fail("Process failed to start")

    process = process_holder[0]

    # Build struct template
    struct_members_template = []
    for struct_item in struct_members_base:
        struct_name = struct_item.get('name')
        if struct_name in configs and struct_name != 'MotorController':
            struct_copy = json.loads(json.dumps(struct_item))
            for field, value in configs[struct_name].get('parameters', {}).items():
                if field in struct_copy.get('parameters', {}):
                    struct_copy['parameters'][field] = value
            struct_members_template.append(struct_copy)
        elif struct_name not in configs:
            struct_members_template.append(struct_item)

    # Start of thread 2
    def generate_json_string(csv_row, row_id):
        test_main.update_mcm_config_from_csv_row(mcm_config, csv_row)

        struct_members = json.loads(json.dumps(struct_members_template))
        mcm_struct_item = next(
            (s for s in struct_members_base if s.get('name') == 'MotorController'),
            None
        )

        if mcm_struct_item:
            mcm_struct = json.loads(json.dumps(mcm_struct_item))
            for field, value in mcm_config.get('parameters', {}).items():
                if field in mcm_struct.get('parameters', {}):
                    mcm_struct['parameters'][field] = value
            struct_members.append(mcm_struct)

        return json.dumps(
            {'row_id': row_id, 'structs': struct_members},
            separators=(',', ':')
        )
    # End of thread 2
    
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

            writer.writerow({
                'MCM Motor Speed': motor_rpm_value,
                **{k: efficiency_data.get(k) for k in fieldnames[1:]}
            })

            results_written += 1
            if save_progress_every and results_written % save_progress_every == 0:
                f.flush()
                print(f"Progress: {results_written}/{len(rows)}")

    stop_event.set()
    process.terminate()
    process_thread.join(timeout=2.0)

    print(f"Completed! {results_written} results written to {output_csv_file}")
