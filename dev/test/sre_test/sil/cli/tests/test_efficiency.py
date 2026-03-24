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

csv_path = DATA / "eff_data.csv"
progress = 10
SIL_CYCLE_TIME_S = 0.01

def _pick(data, *keys):
    """Return first non-None value for any key."""
    for key in keys:
        if key in data and data[key] is not None:
            return data[key]
    return None


def _efficiency_row_from_response(response):
    """
    Normalize SIL efficiency payloads across schema variants.

    Older code emitted snake_case fields under `efficiency`,
    newer code emits camelCase fields under `Efficiency`.
    """
    if not response:
        return {k: None for k in fieldnames}

    eff_data = response.get("Efficiency") or response.get("efficiency") or {}

    return {
        "time_in_straight": _pick(eff_data, "time_in_straight", "straightTime_s"),
        "time_in_corners": _pick(eff_data, "time_in_corners", "cornerTime_s"),
        "lap_counter": _pick(eff_data, "lap_counter", "lapCounter"),
        "pl_target": _pick(eff_data, "pl_target", "plTargetPower"),
        "energy_consumption_per_lap": _pick(
            eff_data, "energy_consumption_per_lap", "lapEnergy_kWh"
        ),
        "energy_budget_per_lap": _pick(
            eff_data, "energy_budget_per_lap", "energyBudget_kWh"
        ),
        "carry_over_energy": _pick(
            eff_data, "carry_over_energy", "carryOverEnergy_kWh"
        ),
        "total_lap_distance": _pick(
            eff_data, "total_lap_distance", "totalLapDistance_km", "lapDistance_km"
        ),
    }


def _resample_rows_to_cycle(rows, cycle_time_s=SIL_CYCLE_TIME_S, time_column="Time"):
    """Downsample rows so replay cadence matches SIL cycle time."""
    if cycle_time_s <= 0:
        return rows

    sampled_rows = []
    next_sample_time = None

    for row in rows:
        time_val = parse_csv_value(row.get(time_column))
        if time_val is None:
            continue
        t = float(time_val)

        if next_sample_time is None:
            sampled_rows.append(row)
            next_sample_time = t + cycle_time_s
            continue

        if t + 1e-9 >= next_sample_time:
            sampled_rows.append(row)
            while next_sample_time <= t + 1e-9:
                next_sample_time += cycle_time_s

    return sampled_rows


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


def test_efficiency1(output_writer):
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
    rows = _resample_rows_to_cycle(rows, cycle_time_s=SIL_CYCLE_TIME_S, time_column="Time")
    total_rows = len(rows)

    # console.print(f"[cyan]Running efficiency test ({total_rows} rows)[/cyan]")

    # Run simulation
    results_written = 0

    try:
        sim.send_structs("PowerLimit", row_id=-1) # row_id is redundant (should be removed)
        
        # Configure requested outputs once; repeating this every row adds avoidable IPC overhead.
        # _ = sim.receive("MotorController", "Efficiency", timeout=2.0)

        for row_idx, csv_row in enumerate(rows):
            json_data = csv_row_to_json(
                csv_row, row_idx, CSV_TO_MCM_PARAMS, CSV_TO_TPS_PARAMS
            )
            sim.send(json_data) 

            # Output request is already configured above, so only receive here.
            response = sim.receive()
            # if response is None:
            #     timeout_rows += 1
            #     response = {}

            # Build result row from efficiency data
            result_row = _efficiency_row_from_response(response)
            
            writer.writerow(result_row)

            results_written += 1
            print_progress(results_written, total_rows, progress, flush=f.flush)
    finally:
        sim.stop()

    console.print(
        f"[green]Completed![/green] {results_written} results written to {output} "
        # f"(failed sends: {failed_rows}, timeouts: {timeout_rows})"
    )


