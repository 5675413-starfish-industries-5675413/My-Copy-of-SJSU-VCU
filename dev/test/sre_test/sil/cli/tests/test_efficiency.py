"""
Efficiency test CLI commands.
"""

import pytest
import csv
import time
from pathlib import Path

import click

from sre_test.sil.cli.console import console
from sre_test.sil.cli.csv_mappings import CSV_TO_MCM_PARAMS, CSV_TO_TPS_PARAMS
from sre_test.sil.core.simulator import SILSimulator
from sre_test.sil.core.helpers.path import DATA
from sre_test.sil.core.helpers.utils import (
    read_csv_rows,
    csv_row_to_json,
    ensure_struct,
    parse_csv_value,
)

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

csv_file = "./sre_test/sil/tests/data/eff_data.csv"
progress = 10

@pytest.fixture(scope="module", autouse=True)
def setup_simulator():
    """Automatically create sim for all tests - no parameter needed."""
    global sim
    sim = SILSimulator.create(auto_compile=True)
    yield
    sim.stop()
    sim = None

def test_efficiency():
    """
    Run closed-loop efficiency test with CSV data.

    Reads motor data from CSV, sends to SIL sim row-by-row,
    and collects efficiency results.
    """
    # Ensure struct definitions exist
    ensure_struct()
    
    # Read CSV file
    csv_file_path = Path(csv_file)
    
    # Ensure data directory exists and set fixed output path
    DATA.mkdir(parents=True, exist_ok=True)
    output = DATA / f"{csv_file_path.stem}_results.csv"
    
    # Read and filter CSV rows
    rows = read_csv_rows(csv_file_path)
    total_rows = len(rows)

    console.print(f"[cyan]Running efficiency test ({total_rows} rows)[/cyan]")

    # Run simulation
    results_written = 0
    failed_rows = 0
    timeout_rows = 0
    with open(output, 'w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()

        try:
            # SILSimulator.create() already starts the process; do not start it again.
            # Send initial configuration structs
            sim.send_structs("PowerLimit", row_id=-1)
            time.sleep(0.1)  # Give it a moment to process
            
            # Configure requested outputs once; repeating this every row adds avoidable IPC overhead.
            _ = sim.receive("MotorController", "Efficiency", timeout=2.0)

            for row_idx, csv_row in enumerate(rows):
                row_number = row_idx + 1
                try:
                    json_data = csv_row_to_json(
                        csv_row, row_idx, CSV_TO_MCM_PARAMS, CSV_TO_TPS_PARAMS
                    )
                    sim.send(json_data) 
                except Exception as e:
                    failed_rows += 1
                    console.print(f"[yellow]Error sending JSON at row {row_number}: {e}[/yellow]")
                    continue

                # Output request is already configured above, so only receive here.
                response = sim.receive(timeout=0.25)
                if response is None:
                    timeout_rows += 1
                    response = {}

                # Build result row from efficiency data
                efficiency_data = response.get("efficiency", {})
                input_motor_rpm = parse_csv_value(csv_row.get("MCM Motor Speed"))

                result_row = {k: efficiency_data.get(k) for k in fieldnames}
                # Keep MCM Motor Speed aligned to the original input CSV row.
                result_row['MCM Motor Speed'] = input_motor_rpm
                
                writer.writerow(result_row)

                results_written += 1
                if progress and results_written % progress == 0:
                    f.flush()
                    progress_pct = (results_written / total_rows * 100) if total_rows else 0.0
                    console.print(
                        f"[cyan]Progress: {results_written}/{total_rows} ({progress_pct:.1f}%)[/cyan]" #for some reason its purple
                    )
        finally:
            sim.stop()

    console.print(
        f"[green]Completed![/green] {results_written} results written to {output} "
        f"(failed sends: {failed_rows}, timeouts: {timeout_rows})"
    )

