#!/usr/bin/env python3
"""
Plot efficiency metrics from eff_results.csv against time from eff_data.csv
"""

import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path

# Get script directory
script_dir = Path(__file__).parent
results_csv = script_dir / 'csv_files' / 'eff_results.csv'
data_csv = script_dir / 'csv_files' / 'eff_data.csv'
output_dir = script_dir / 'csv_files' / 'plots'
output_dir.mkdir(exist_ok=True)

print(f"Loading results from {results_csv}...")
df_results = pd.read_csv(results_csv)

print(f"Loading time data from {data_csv}...")
# Read CSV with proper header handling
df_data = pd.read_csv(data_csv)

# Extract Time column from data CSV (skip header, unit, and empty rows)
time_values = []
for idx, row in df_data.iterrows():
    try:
        time_str = str(row['Time']).strip().strip('"')
        # Skip header, unit rows, and empty values
        if time_str and time_str.lower() not in ['time', 's', ''] and time_str != 'nan':
            time_val = float(time_str)
            time_values.append(time_val)
    except (ValueError, KeyError, TypeError):
        continue

# Ensure we have the same number of rows
min_rows = min(len(df_results), len(time_values))
df_results = df_results.iloc[:min_rows].copy()
time_values = time_values[:min_rows]

# Add time column to results dataframe
df_results['Time'] = time_values

print(f"Loaded {len(df_results)} rows")
print(f"Columns: {list(df_results.columns)}")

# Set up the plotting style
plt.style.use('seaborn-v0_8-darkgrid')
fig_size = (16, 10)

# ============================================================================
# Plot: Comprehensive Overview (All metrics in one figure)
# ============================================================================
fig, axes = plt.subplots(4, 2, figsize=(18, 16))
fig.suptitle('Efficiency Metrics Overview', fontsize=18, fontweight='bold')

# Row 1: Time metrics
axes[0, 0].plot(df_results['Time'], df_results['time_in_straight'], 
                label='Time in Straights', color='blue', linewidth=1)
axes[0, 0].set_ylabel('Time (s)')
axes[0, 0].set_title('Time in Straights')
axes[0, 0].legend()
axes[0, 0].grid(True, alpha=0.3)

axes[0, 1].plot(df_results['Time'], df_results['time_in_corners'], 
                label='Time in Corners', color='red', linewidth=1)
axes[0, 1].set_ylabel('Time (s)')
axes[0, 1].set_title('Time in Corners')
axes[0, 1].legend()
axes[0, 1].grid(True, alpha=0.3)

# Row 2: Lap and Power
axes[1, 0].plot(df_results['Time'], df_results['lap_counter'], 
                label='Lap Counter', color='green', linewidth=1)
axes[1, 0].set_ylabel('Lap Number')
axes[1, 0].set_title('Lap Counter')
axes[1, 0].legend()
axes[1, 0].grid(True, alpha=0.3)

axes[1, 1].plot(df_results['Time'], df_results['pl_target'], 
                label='PL Target', color='purple', linewidth=1)
axes[1, 1].set_ylabel('Power (kW)')
axes[1, 1].set_title('Power Limit Target')
axes[1, 1].legend()
axes[1, 1].grid(True, alpha=0.3)

# Row 3: Energy metrics
axes[2, 0].plot(df_results['Time'], df_results['energy_consumption_per_lap'], 
                label='Consumption', color='red', linewidth=1)
axes[2, 0].plot(df_results['Time'], df_results['energy_budget_per_lap'], 
                label='Budget', color='blue', linewidth=1, alpha=0.7)
axes[2, 0].set_ylabel('Energy (kWh)')
axes[2, 0].set_title('Energy per Lap')
axes[2, 0].legend()
axes[2, 0].grid(True, alpha=0.3)

axes[2, 1].plot(df_results['Time'], df_results['carry_over_energy'], 
                label='Carry Over', color='green', linewidth=1)
axes[2, 1].set_ylabel('Energy (kWh)')
axes[2, 1].set_title('Carry Over Energy')
axes[2, 1].legend()
axes[2, 1].grid(True, alpha=0.3)

# Row 4: Distance
axes[3, 0].plot(df_results['Time'], df_results['total_lap_distance'], 
                label='Total Distance', color='orange', linewidth=1)
axes[3, 0].set_xlabel('Time (s)')
axes[3, 0].set_ylabel('Distance (km)')
axes[3, 0].set_title('Total Lap Distance')
axes[3, 0].legend()
axes[3, 0].grid(True, alpha=0.3)

# Empty subplot for symmetry
axes[3, 1].axis('off')

plt.tight_layout()
plt.savefig(output_dir / 'efficiency_metrics_overview.png', dpi=300, bbox_inches='tight')
print(f"Saved: {output_dir / 'efficiency_metrics_overview.png'}")
plt.close()

print(f"\nAll plots saved to: {output_dir}")
print("Generated plots:")
print("  1. time_analysis.png - Time in straights and corners")
print("  2. lap_counter.png - Lap counter over time")
print("  3. pl_target.png - Power limit target over time")
print("  4. energy_consumption_per_lap.png - Energy consumption per lap")
print("  5. energy_budget_per_lap.png - Energy budget per lap")
print("  6. carry_over_energy.png - Carry over energy")
print("  7. total_lap_distance.png - Total lap distance")
print("  8. efficiency_metrics_overview.png - All metrics in one view")

