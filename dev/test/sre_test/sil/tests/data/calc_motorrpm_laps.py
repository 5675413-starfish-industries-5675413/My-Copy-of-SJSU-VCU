#!/usr/bin/env python3
"""
Quick utility: estimate lap count from motor RPM CSV data.

Uses the same conversion as `MCM_getGroundSpeedKPH()` in `motorController.c`:
    ground_kph = ((motorRPM / FD_Ratio) * 60 * tire_circ_m) / 1000
"""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


def estimate_laps_from_csv(
    csv_path: Path,
    rpm_col: str = "MCM Motor Speed",
    time_col: str = "Time",
    fd_ratio: float = 3.2,
    tire_circ_m: float = 1.295,
    lap_length_km: float = 1.0,
    cycle_time_s: float | None = None,
) -> tuple[float, float, int]:
    """
    Return: (distance_km, raw_laps, completed_laps)
    """
    rows = []
    with open(csv_path, "r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            t_raw = (row.get(time_col) or "").strip().strip('"')
            rpm_raw = (row.get(rpm_col) or "").strip().strip('"')
            if not t_raw or not rpm_raw:
                continue
            try:
                t = float(t_raw)
                rpm = float(rpm_raw)
            except ValueError:
                continue
            rows.append((t, rpm))

    if len(rows) < 2:
        raise ValueError("Not enough valid rows to integrate distance.")

    distance_km = 0.0

    for i in range(1, len(rows)):
        t_prev, _ = rows[i - 1]
        t_cur, rpm = rows[i]
        dt = cycle_time_s if cycle_time_s is not None else (t_cur - t_prev)
        if dt <= 0:
            continue

        # Same model as firmware (motorController.c)
        ground_kph = ((rpm / fd_ratio) * 60.0 * tire_circ_m) / 1000.0
        distance_km += (ground_kph * dt) / 3600.0

    raw_laps = distance_km / lap_length_km
    completed_laps = int(distance_km // lap_length_km)
    return distance_km, raw_laps, completed_laps


def main() -> None:
    parser = argparse.ArgumentParser(description="Estimate laps from motor RPM CSV.")
    parser.add_argument(
        "--csv",
        type=Path,
        default=Path(__file__).parent / "eff_data.csv",
        help="Path to source CSV (default: eff_data.csv next to this script).",
    )
    parser.add_argument("--lap-km", type=float, default=1.0, help="Lap length in km.")
    parser.add_argument("--fd-ratio", type=float, default=3.2, help="Final drive ratio.")
    parser.add_argument("--tire-circ", type=float, default=1.295, help="Tire circumference in meters.")
    parser.add_argument(
        "--cycle-time",
        type=float,
        default=None,
        help="Force fixed dt (seconds) instead of CSV time deltas, e.g. 0.01",
    )
    args = parser.parse_args()

    distance_km, raw_laps, completed_laps = estimate_laps_from_csv(
        csv_path=args.csv,
        lap_length_km=args.lap_km,
        fd_ratio=args.fd_ratio,
        tire_circ_m=args.tire_circ,
        cycle_time_s=args.cycle_time,
    )

    print(f"CSV: {args.csv}")
    print(f"Estimated distance: {distance_km:.6f} km")
    print(f"Raw laps (@ {args.lap_km:.3f} km/lap): {raw_laps:.6f}")
    print(f"Completed laps: {completed_laps}")


if __name__ == "__main__":
    main()
