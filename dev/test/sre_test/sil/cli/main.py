"""
SIL CLI Commands
"""

import click
from pathlib import Path

try:
    from rich.console import Console
    console = Console()
except ImportError:
    # Fallback if rich is not available
    class Console:
        def print(self, *args, **kwargs):
            print(*args, **kwargs)
    console = Console()


@click.group()
def cli():
    """
    Software-in-the-Loop Testing for VCU

    Compile and run VCU firmware in simulation mode.
    """
    pass


@cli.command()
@click.option('--verbose', '-v', is_flag=True, help='Show compilation output')
@click.option('--output-mode', type=str, default='0x01', help='SIL output mode (0x01=efficiency, 0x07=all)')
def build(verbose, output_mode):
    """Compile the SIL executable."""
    from sre_test.sil.core.compiler import SILCompiler

    # Parse output mode
    try:
        sil_output_mode = int(output_mode, 0)  # Allow hex (0x01) or decimal (1)
    except ValueError:
        console.print(f"[red]Invalid output mode: {output_mode}[/red]")
        raise SystemExit(1)

    console.print("[cyan]Compiling SIL executable...[/cyan]")

    compiler = SILCompiler(sil_output_mode=sil_output_mode)
    if compiler.compile(verbose=verbose):
        console.print(f"[green]Success![/green] Executable: {compiler.executable}")
    else:
        console.print("[red]Compilation failed[/red]")
        raise SystemExit(1)


@cli.command()
@click.option('--output', '-o', type=click.Path(), help='Output JSON file')
def extract(output):
    """Extract struct definitions from C headers."""
    from sre_test.sil.core.struct_parser import extract_struct_definitions

    # Determine paths
    script_dir = Path(__file__).parent.parent  # dev/test/sre_test/sil/
    sre_test_dir = script_dir.parent  # dev/test/sre_test/
    test_dir = sre_test_dir.parent  # dev/test/
    dev_dir = test_dir.parent / 'dev'
    root_dir = dev_dir.parent
    inc_dir = root_dir / 'inc'
    
    # Determine output file
    if output:
        output_file = Path(output)
    else:
        config_dir = script_dir / 'config'
        config_dir.mkdir(exist_ok=True)
        output_file = config_dir / 'struct_members_output.json'

    console.print("[cyan]Extracting struct definitions...[/cyan]")
    
    try:
        extract_struct_definitions(dev_dir, inc_dir, output_file)
        console.print(f"[green]Done![/green] Output written to {output_file}")
    except Exception as e:
        console.print(f"[red]Error: {e}[/red]")
        raise SystemExit(1)


@cli.command()
@click.option('--config', '-c', type=click.Path(exists=True), help='Config JSON file')
@click.option('--timeout', '-t', type=float, default=2.0, help='Run timeout in seconds')
def run(config, timeout):
    """Run a single SIL simulation."""
    from sre_test.sil.core.compiler import SILCompiler
    from sre_test.sil.core.simulator import SILSimulator
    import json

    compiler = SILCompiler()
    if not compiler.executable.exists():
        console.print("[yellow]Executable not found. Building...[/yellow]")
        if not compiler.compile():
            console.print("[red]Build failed[/red]")
            raise SystemExit(1)

    console.print("[cyan]Starting SIL simulation...[/cyan]")
    
    with SILSimulator(compiler.executable) as sim:
        # Load config if provided
        if config:
            with open(config, 'r') as f:
                config_data = json.load(f)
                sim.send(config_data)
                console.print(f"[green]Sent config: {config}[/green]")
        
        # Wait for and display output
        response = sim.receive(timeout=timeout)
        if response:
            console.print("[green]Response:[/green]")
            console.print(json.dumps(response, indent=2))
        else:
            console.print("[yellow]No response received within timeout[/yellow]")


@cli.command()
@click.argument('csv_file', type=click.Path(exists=True))
@click.option('--output', '-o', type=click.Path(), help='Output CSV file')
@click.option('--config', '-c', type=click.Path(exists=True), help='PowerLimit config')
@click.option('--timeout-per-row', type=float, default=2.0, help='Timeout per CSV row (seconds)')
@click.option('--save-progress-every', type=int, default=100, help='Save progress every N rows')
@click.option('--output-mode', type=str, default='0x01', help='SIL output mode')
def efficiency(csv_file, output, config, timeout_per_row, save_progress_every, output_mode):
    """
    Run closed-loop efficiency test with CSV data.

    Reads motor data from CSV, sends to SIL simulator row-by-row,
    and collects efficiency results.
    """
    from sre_test.sil.core.compiler import SILCompiler
    from sre_test.sil.core.simulator import SILSimulator
    from sre_test.sil.core.struct_parser import extract_struct_definitions
    import csv
    import json
    import time

    # Parse output mode
    try:
        sil_output_mode = int(output_mode, 0)
    except ValueError:
        console.print(f"[red]Invalid output mode: {output_mode}[/red]")
        raise SystemExit(1)

    # Determine paths
    script_dir = Path(__file__).parent.parent  # dev/test/sre_test/sil/
    sre_test_dir = script_dir.parent  # dev/test/sre_test/
    test_dir = sre_test_dir.parent  # dev/test/
    dev_dir = test_dir.parent / 'dev'
    root_dir = dev_dir.parent
    inc_dir = root_dir / 'inc'
    
    config_dir = script_dir / 'config'
    config_dir.mkdir(exist_ok=True)
    struct_members_path = config_dir / 'struct_members_output.json'
    
    # Extract struct definitions if needed
    if not struct_members_path.exists():
        console.print("[cyan]Extracting struct definitions...[/cyan]")
        extract_struct_definitions(dev_dir, inc_dir, struct_members_path)
    
    # Read CSV file
    csv_file_path = Path(csv_file)
    
    # Determine data directory for output
    data_dir = script_dir / 'tests' / 'data'
    data_dir.mkdir(parents=True, exist_ok=True)
    
    if output:
        output_path = Path(output)
        # If it's a relative path (not absolute), resolve it relative to data_dir
        if not output_path.is_absolute():
            output_csv_file = data_dir / output_path.name
        else:
            output_csv_file = output_path
    else:
        # Default to data folder with _results suffix
        output_csv_file = data_dir / f"{csv_file_path.stem}_results.csv"
    
    with open(csv_file_path, 'r', encoding='utf-8') as f:
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
        console.print("[red]No data rows found in CSV file[/red]")
        raise SystemExit(1)

    # Load struct members
    with open(struct_members_path, 'r') as f:
        struct_members_base = json.load(f)

    # Load PowerLimit config if provided
    pl_config_struct = None
    if config:
        with open(config, 'r') as f:
            pl_config_data = json.load(f)
            if pl_config_data.get("name") == "PowerLimit":
                pl_config_struct = {
                    "name": "PowerLimit",
                    "parameters": pl_config_data.get("parameters", {})
                }

    # Compile
    console.print("[cyan]Compiling SIL executable...[/cyan]")
    compiler = SILCompiler(sil_output_mode=sil_output_mode)
    if not compiler.executable.exists():
        if not compiler.compile(verbose=True):
            console.print("[red]Build failed[/red]")
            raise SystemExit(1)

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

    console.print(f"[cyan]Running efficiency test with {csv_file_path}...[/cyan]")
    
    # CSV column mappings
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

        return {"row_id": row_id, "structs": structs}

    # Run simulation
    results_written = 0
    with open(output_csv_file, 'w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

        with SILSimulator(compiler.executable) as sim:
            # Send PowerLimit config once at startup if available
            if pl_config_struct:
                pl_init_json = {"row_id": -1, "structs": [pl_config_struct]}
                sim.send(pl_init_json)
                time.sleep(0.1)  # Give it a moment to process

            for row_idx, csv_row in enumerate(rows):
                try:
                    sim.send(generate_json_string(csv_row, row_idx))
                except Exception as e:
                    console.print(f"[yellow]Error sending JSON at row {row_idx}: {e}[/yellow]")
                    continue

                efficiency_data = {}
                response = sim.receive(timeout=timeout_per_row)
                if response:
                    efficiency_data = response.get('efficiency', {})
                elif response is None:
                    console.print(f"[yellow]Timeout waiting for response at row {row_idx}[/yellow]")

                motor_rpm = csv_row.get('MCM Motor Speed', '').strip().strip('"')
                try:
                    motor_rpm_value = float(motor_rpm) if motor_rpm else None
                except ValueError:
                    motor_rpm_value = None

                # Build result row
                result_row = {k: efficiency_data.get(k) for k in fieldnames}
                result_row['MCM Motor Speed'] = motor_rpm_value
                writer.writerow(result_row)

                results_written += 1
                if save_progress_every and results_written % save_progress_every == 0:
                    f.flush()
                    console.print(f"[cyan]Progress: {results_written}/{len(rows)}[/cyan]")

    console.print(f"[green]Completed![/green] {results_written} results written to {output_csv_file}")


@cli.command()
@click.option('-k', 'expression', help='Only run tests matching expression')
@click.option('-v', '--verbose', is_flag=True, help='Verbose output')
def test(expression, verbose):
    """Run SIL pytest tests."""
    import subprocess

    cmd = ['pytest', 'sre_test/sil/tests/']
    if expression:
        cmd.extend(['-k', expression])
    if verbose:
        cmd.append('-v')

    subprocess.run(cmd)

