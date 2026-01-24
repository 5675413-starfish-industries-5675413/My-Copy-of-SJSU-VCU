#!/usr/bin/env python3
"""
Quick plotting script for my_results.csv
"""

import csv
import matplotlib.pyplot as plt
from pathlib import Path

# Get the CSV file path (same directory as this script)
script_dir = Path(__file__).parent
csv_file = script_dir / 'my_results.csv'

# Read the CSV
data = {}
with open(csv_file, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        for key, value in row.items():
            if key not in data:
                data[key] = []
            try:
                data[key].append(float(value))
            except ValueError:
                data[key].append(value)

# Create time index
n_samples = len(data[list(data.keys())[0]])
time = list(range(n_samples))

# Create subplots
fig, axes = plt.subplots(3, 3, figsize=(15, 12))
fig.suptitle('SIL Test Results', fontsize=16)

# Plot 1: Motor Speed
axes[0, 0].plot(time, data['MCM Motor Speed'])
axes[0, 0].set_title('Motor Speed')
axes[0, 0].set_xlabel('Sample')
axes[0, 0].set_ylabel('RPM')
axes[0, 0].grid(True)

# Plot 2: Time in Straight
axes[0, 1].plot(time, data['time_in_straight'])
axes[0, 1].set_title('Time in Straight')
axes[0, 1].set_xlabel('Sample')
axes[0, 1].set_ylabel('Time (s)')
axes[0, 1].grid(True)

# Plot 3: Time in Corners
axes[0, 2].plot(time, data['time_in_corners'])
axes[0, 2].set_title('Time in Corners')
axes[0, 2].set_xlabel('Sample')
axes[0, 2].set_ylabel('Time (s)')
axes[0, 2].grid(True)

# Plot 4: Lap Counter
axes[1, 0].plot(time, data['lap_counter'])
axes[1, 0].set_title('Lap Counter')
axes[1, 0].set_xlabel('Sample')
axes[1, 0].set_ylabel('Lap #')
axes[1, 0].grid(True)

# Plot 5: Power Limit Target
axes[1, 1].plot(time, data['pl_target'])
axes[1, 1].set_title('Power Limit Target')
axes[1, 1].set_xlabel('Sample')
axes[1, 1].set_ylabel('Power (kW)')
axes[1, 1].grid(True)

# Plot 6: Energy Consumption per Lap
axes[1, 2].plot(time, data['energy_consumption_per_lap'])
axes[1, 2].set_title('Energy Consumption per Lap')
axes[1, 2].set_xlabel('Sample')
axes[1, 2].set_ylabel('Energy (kWh)')
axes[1, 2].grid(True)

# Plot 7: Energy Budget per Lap
axes[2, 0].plot(time, data['energy_budget_per_lap'])
axes[2, 0].set_title('Energy Budget per Lap')
axes[2, 0].set_xlabel('Sample')
axes[2, 0].set_ylabel('Energy (kWh)')
axes[2, 0].grid(True)

# Plot 8: Carry Over Energy
axes[2, 1].plot(time, data['carry_over_energy'])
axes[2, 1].set_title('Carry Over Energy')
axes[2, 1].set_xlabel('Sample')
axes[2, 1].set_ylabel('Energy (kWh)')
axes[2, 1].grid(True)

# Plot 9: Total Lap Distance
axes[2, 2].plot(time, data['total_lap_distance'])
axes[2, 2].set_title('Total Lap Distance')
axes[2, 2].set_xlabel('Sample')
axes[2, 2].set_ylabel('Distance (km)')
axes[2, 2].grid(True)

plt.tight_layout()
plt.savefig(script_dir / 'results_plot.png', dpi=150)
print(f"Plot saved to: {script_dir / 'results_plot.png'}")
plt.show()

