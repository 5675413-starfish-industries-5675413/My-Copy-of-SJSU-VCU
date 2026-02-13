"""
Efficiency test CLI commands.
"""

import csv
import time
from pathlib import Path

import click

from sre_test.sil.cli.console import console
from sre_test.sil.core.simulator import SILSimulator
from sre_test.sil.core.helpers.path import DATA
from sre_test.sil.core.helpers.utils import (
    read_csv_rows,
    csv_row_to_json,
    ensure_struct_definitions,
)

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

@click.command()
@click.argument('csv_file', type=click.Path(exists=True))
@click.option('--timeout-per-row', type=float, default=2.0, help='Timeout per CSV row (seconds)')
@click.option('--save-progress-every', type=int, default=100, help='Save progress every N rows')
def efficiency(csv_file, timeout_per_row, save_progress_every):
    """
    Run closed-loop efficiency test with CSV data.

    Reads motor data from CSV, sends to SIL simulator row-by-row,
    and collects efficiency results.
    """
    # Ensure struct definitions exist
    ensure_struct_definitions()
    
    # Read CSV file
    csv_file_path = Path(csv_file)
    
    # Ensure data directory exists and set fixed output path
    DATA.mkdir(parents=True, exist_ok=True)
    output_csv_file = DATA / f"{csv_file_path.stem}_results.csv"
    
    # Read and filter CSV rows
    rows = read_csv_rows(csv_file_path)

    # Create simulator (auto-compiles if needed)
    console.print("[cyan]Compiling SIL executable...[/cyan]")
    
    try:
        simulator = SILSimulator.create(
            verbose=True,
            auto_compile=False
        )
    except RuntimeError:
        console.print("[red]Build failed[/red]")
        raise SystemExit(1)

    console.print(f"[cyan]Running efficiency test[/cyan]")

    # Run simulation
    results_written = 0
    with open(output_csv_file, 'w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

        with simulator as sim:
            # Send initial configuration structs
            sim.send_structs("PowerLimit", row_id=-1)
            time.sleep(0.1)  # Give it a moment to process
            
            _ = sim.receive("Efficiency", "PowerLimit", timeout=2.0)

            for row_idx, csv_row in enumerate(rows):
                try:
                    json_data = csv_row_to_json(
                        csv_row, row_idx, CSV_TO_MCM_PARAMS, CSV_TO_TPS_PARAMS
                    )
                    sim.send(json_data) 
                except Exception as e:
                    console.print(f"[yellow]Error sending JSON at row {row_idx}: {e}[/yellow]")
                    continue

                response = sim.receive("MotorController", "Efficiency", timeout=timeout_per_row)
                if response is None:
                    console.print(f"[yellow]Timeout waiting for response at row {row_idx}[/yellow]")
                    response = {}

                # Build result row from efficiency data
                efficiency_data = response.get("efficiency", {})
                motor_rpm_value = response.get("motor_controller", {}).get("motor_rpm")

                result_row = {k: efficiency_data.get(k) for k in fieldnames}
                result_row['MCM Motor Speed'] = motor_rpm_value
                
                print(result_row)
                writer.writerow(result_row)

                results_written += 1
                if save_progress_every and results_written % save_progress_every == 0:
                    f.flush()
                    console.print(f"[cyan]Progress: {results_written}/{len(rows)}[/cyan]")

    console.print(f"[green]Completed![/green] {results_written} results written to {output_csv_file}")

