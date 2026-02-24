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
    print_progress,
)

# Output CSV columns
fieldnames = [
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
csv_path = Path(csv_file)
progress = 10

@pytest.fixture(scope="module", autouse=True)
def setup_simulator():
    """Automatically create sim for all tests - no parameter needed."""
    global sim
    sim = SILSimulator.create(auto_compile=True)
    yield
    sim.stop()
    sim = None

@pytest.fixture
def output_writer():
    """Provide opened output CSV file and writer for efficiency test."""

    DATA.mkdir(parents=True, exist_ok=True)
    output = DATA / f"{csv_path.stem}_results.csv"

    with open(output, 'w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        yield output, f, writer


def test_efficiency(output_writer):
    """
    Run closed-loop efficiency test with CSV data.

    Reads motor data from CSV, sends to SIL sim row-by-row,
    and collects efficiency results.
    """
    # Ensure struct definitions exist
    ensure_struct()
    
    output, f, writer = output_writer
    
    # Read and filter CSV rows
    rows = read_csv_rows(csv_path)
    total_rows = len(rows)

    console.print(f"[cyan]Running efficiency test ({total_rows} rows)[/cyan]")

    # Run simulation
    results_written = 0
    failed_rows = 0
    timeout_rows = 0
    try:
        sim.send_structs("PowerLimit", row_id=-1)
        time.sleep(0.1)  # Give it a moment to process
        
        # Configure requested outputs once; repeating this every row adds avoidable IPC overhead.
        # _ = sim.receive("MotorController", "Efficiency", timeout=2.0)

        for row_idx, csv_row in enumerate(rows):
            row_number = row_idx + 1
            json_data = csv_row_to_json(
                csv_row, row_idx, CSV_TO_MCM_PARAMS, CSV_TO_TPS_PARAMS
            )
            sim.send(json_data) 

            # Output request is already configured above, so only receive here.
            response = sim.receive(timeout=0.25)
            if response is None:
                timeout_rows += 1
                response = {}

            # Build result row from efficiency data
            data = response.get("efficiency", {})
            result_row = {k: data.get(k) for k in fieldnames}
            
            writer.writerow(result_row)

            results_written += 1
            print_progress(results_written, total_rows, progress, flush=f.flush)
    finally:
        sim.stop()

    console.print(
        f"[green]Completed![/green] {results_written} results written to {output} "
        f"(failed sends: {failed_rows}, timeouts: {timeout_rows})"
    )

