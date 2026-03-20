"""
Utility functions for SIL testing.
"""

import csv
import time
from pathlib import Path
from typing import Optional, Dict, Any, List, Union, Callable

from sre_test.sil.cli.console import console
from sre_test.sil.core.struct_parser import extract_struct_definitions
from sre_test.sil.core.helpers.path import CONFIG, STRUCT_MEMBERS

def _is_valid_float(value: str) -> bool:
    """Check if a string can be converted to a float."""
    try:
        float(value)
        return True
    except ValueError:
        return False


def read_csv_rows(csv_file: Path, time_column: str = 'Time') -> List[Dict[str, str]]:
    """
    Read CSV and filter rows with valid time values.
    
    Args:
        csv_file: Path to CSV file
        time_column: Name of the time column to validate
    
    Returns:
        List of CSV row dictionaries with valid time values
    
    Raises:
        SystemExit: If no data rows are found in the CSV file
    """
    rows = []
    with open(csv_file, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            time_value = row.get(time_column, '').strip().strip('"')
            if time_value and _is_valid_float(time_value):
                rows.append(row)
    if not rows:
        console.print("[red]No data rows found in CSV file[/red]")
        raise SystemExit(1)
    return rows


def parse_csv_value(value: Any) -> Optional[Union[int, float]]:
    """
    Parse CSV value to int or float, returning None if invalid.
    
    Args:
        value: Value to parse (can be string, number, or None)
    
    Returns:
        Parsed int or float, or None if invalid
    """
    if value is None:
        return None
    s = str(value).strip().strip('"')
    if not s:
        return None
    try:
        f = float(s)
        return int(f) if f.is_integer() else f
    except ValueError:
        return None


def csv_row_to_json(
    csv_row: Dict[str, str],
    row_id: int,
    mcm_mapping: Dict[str, str],
    tps_mapping: Dict[str, str]
) -> Dict[str, Any]:
    """
    Convert CSV row to JSON format for SIL simulator.
    
    Args:
        csv_row: CSV row dictionary
        row_id: Row ID to include in JSON
        mcm_mapping: Mapping from CSV columns to MotorController parameters
        tps_mapping: Mapping from CSV columns to TorqueEncoder parameters
    
    Returns:
        JSON dictionary with row_id and structs
    """
    structs = []
    
    # MotorController parameters
    mcm_params = {}
    for csv_key, param_key in mcm_mapping.items():
        val = parse_csv_value(csv_row.get(csv_key))
        if val is not None:
            mcm_params[param_key] = val
    if mcm_params:
        structs.append({"name": "MotorController", "parameters": mcm_params})
    
    # TorqueEncoder parameters
    tps_params = {}
    for csv_key, param_key in tps_mapping.items():
        val = parse_csv_value(csv_row.get(csv_key))
        if val is not None:
            tps_params[param_key] = val
    if tps_params:
        structs.append({"name": "TorqueEncoder", "parameters": tps_params})
    
    return {"row_id": row_id, "structs": structs}


def ensure_struct() -> None:
    """
    Ensure struct definitions file exists, extracting if needed.
    """
    CONFIG.mkdir(exist_ok=True)
    if not STRUCT_MEMBERS.exists():
        console.print("[cyan]Extracting struct definitions...[/cyan]")
        extract_struct_definitions()


def print_progress(
    processed: int,
    total: int,
    every: int,
    flush: Optional[Callable[[], None]] = None,
) -> None:
    """
    Print periodic progress updates.

    Args:
        processed: Number of processed rows
        total: Total number of rows
        every: Print interval (disabled if <= 0)
        flush: Optional callback (for example, file.flush) before printing
    """
    if every <= 0 or processed % every != 0:
        return
    if flush is not None:
        flush()
    progress_pct = (processed / total * 100) if total else 0.0
    console.print(f"[cyan]Progress: {processed}/{total} ({progress_pct:.1f}%)[/cyan]")

