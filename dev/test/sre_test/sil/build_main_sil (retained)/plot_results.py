#!/usr/bin/env python3
"""
Plot graphs from eff_results.csv
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

# Get script directory
script_dir = Path(__file__).parent
csv_file = script_dir / 'csv_files' / 'eff_results.csv'
output_dir = script_dir / 'csv_files' / 'plots'
output_dir.mkdir(exist_ok=True)

print(f"Loading data from {csv_file}...")
df = pd.read_csv(csv_file)

# Convert Time to numeric (remove quotes if present)
df['Time'] = df['Time'].astype(str).str.strip('"').astype(float)

print(f"Loaded {len(df)} rows")
print(f"Columns: {list(df.columns)}")

# Set up the plotting style
plt.style.use('seaborn-v0_8-darkgrid')
fig_size = (16, 10)

# ============================================================================
# Plot 1: Motor Controller Metrics Over Time
# ============================================================================
fig, axes = plt.subplots(2, 2, figsize=fig_size)
fig.suptitle('Motor Controller Metrics Over Time', fontsize=16, fontweight='bold')

# Torque Command and Feedback
ax1 = axes[0, 0]
ax1.plot(df['Time'], df['MCM Torque Command'], label='Torque Command', linewidth=1.5)
ax1.plot(df['Time'], df['MCM Torque Feedback'], label='Torque Feedback', linewidth=1.5, alpha=0.7)
ax1.set_xlabel('Time (s)')
ax1.set_ylabel('Torque (NM)')
ax1.set_title('Torque Command vs Feedback')
ax1.legend()
ax1.grid(True, alpha=0.3)

# Motor Speed
ax2 = axes[0, 1]
ax2.plot(df['Time'], df['MCM Motor Speed'], label='Motor Speed', color='green', linewidth=1.5)
ax2.set_xlabel('Time (s)')
ax2.set_ylabel('Speed (rpm)')
ax2.set_title('Motor Speed Over Time')
ax2.legend()
ax2.grid(True, alpha=0.3)

# DC Bus Voltage
ax3 = axes[1, 0]
ax3.plot(df['Time'], df['MCM DC Bus Voltage'], label='DC Voltage', color='red', linewidth=1.5)
ax3.set_xlabel('Time (s)')
ax3.set_ylabel('Voltage (V)')
ax3.set_title('DC Bus Voltage Over Time')
ax3.legend()
ax3.grid(True, alpha=0.3)

# DC Bus Current
ax4 = axes[1, 1]
ax4.plot(df['Time'], df['MCM DC Bus Current'], label='DC Current', color='orange', linewidth=1.5)
ax4.set_xlabel('Time (s)')
ax4.set_ylabel('Current (A)')
ax4.set_title('DC Bus Current Over Time')
ax4.legend()
ax4.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig(output_dir / 'motor_controller_metrics.png', dpi=300, bbox_inches='tight')
print(f"Saved: {output_dir / 'motor_controller_metrics.png'}")
plt.close()

# ============================================================================
# Plot 2: Throttle Position Over Time
# ============================================================================
fig, ax = plt.subplots(figsize=fig_size)
ax.plot(df['Time'], df['TPS0ThrottlePercent0FF'], label='TPS0', linewidth=1.5)
ax.plot(df['Time'], df['TPS1ThrottlePercent0FF'], label='TPS1', linewidth=1.5, alpha=0.7)
ax.set_xlabel('Time (s)')
ax.set_ylabel('Throttle Position (%)')
ax.set_title('Throttle Position Sensors Over Time')
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig(output_dir / 'throttle_position.png', dpi=300, bbox_inches='tight')
print(f"Saved: {output_dir / 'throttle_position.png'}")
plt.close()

# ============================================================================
# Plot 3: Energy Metrics
# ============================================================================
fig, axes = plt.subplots(2, 2, figsize=fig_size)
fig.suptitle('Energy and Efficiency Metrics', fontsize=16, fontweight='bold')

# Energy Consumption per Lap
ax1 = axes[0, 0]
ax1.plot(df['Time'], df['energy_consumption_per_lap'], label='Energy Consumption', color='red', linewidth=1.5)
ax1.set_xlabel('Time (s)')
ax1.set_ylabel('Energy (kWh)')
ax1.set_title('Energy Consumption per Lap')
ax1.legend()
ax1.grid(True, alpha=0.3)

# Energy Budget per Lap
ax2 = axes[0, 1]
ax2.plot(df['Time'], df['energy_budget_per_lap'], label='Energy Budget', color='blue', linewidth=1.5)
ax2.set_xlabel('Time (s)')
ax2.set_ylabel('Energy (kWh)')
ax2.set_title('Energy Budget per Lap')
ax2.legend()
ax2.grid(True, alpha=0.3)

# Carry Over Energy
ax3 = axes[1, 0]
ax3.plot(df['Time'], df['carry_over_energy'], label='Carry Over Energy', color='green', linewidth=1.5)
ax3.set_xlabel('Time (s)')
ax3.set_ylabel('Energy (kWh)')
ax3.set_title('Carry Over Energy')
ax3.legend()
ax3.grid(True, alpha=0.3)

# Energy Budget vs Consumption
ax4 = axes[1, 1]
ax4.plot(df['Time'], df['energy_budget_per_lap'], label='Budget', color='blue', linewidth=1.5)
ax4.plot(df['Time'], df['energy_consumption_per_lap'], label='Consumption', color='red', linewidth=1.5, alpha=0.7)
ax4.set_xlabel('Time (s)')
ax4.set_ylabel('Energy (kWh)')
ax4.set_title('Energy Budget vs Consumption')
ax4.legend()
ax4.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig(output_dir / 'energy_metrics.png', dpi=300, bbox_inches='tight')
print(f"Saved: {output_dir / 'energy_metrics.png'}")
plt.close()

# ============================================================================
# Plot 4: Power Limit Target
# ============================================================================
fig, ax = plt.subplots(figsize=fig_size)
ax.plot(df['Time'], df['pl_target'], label='PL Target Power', color='purple', linewidth=1.5)
ax.set_xlabel('Time (s)')
ax.set_ylabel('Power (kW)')
ax.set_title('Power Limit Target Over Time')
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig(output_dir / 'power_limit_target.png', dpi=300, bbox_inches='tight')
print(f"Saved: {output_dir / 'power_limit_target.png'}")
plt.close()

# ============================================================================
# Plot 5: Time in Straights vs Corners
# ============================================================================
fig, axes = plt.subplots(1, 2, figsize=fig_size)
fig.suptitle('Time Analysis', fontsize=16, fontweight='bold')

# Time in Straights
ax1 = axes[0]
ax1.plot(df['Time'], df['time_in_straight'], label='Time in Straights', color='blue', linewidth=1.5)
ax1.set_xlabel('Time (s)')
ax1.set_ylabel('Time (s)')
ax1.set_title('Time in Straights')
ax1.legend()
ax1.grid(True, alpha=0.3)

# Time in Corners
ax2 = axes[1]
ax2.plot(df['Time'], df['time_in_corners'], label='Time in Corners', color='red', linewidth=1.5)
ax2.set_xlabel('Time (s)')
ax2.set_ylabel('Time (s)')
ax2.set_title('Time in Corners')
ax2.legend()
ax2.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig(output_dir / 'time_analysis.png', dpi=300, bbox_inches='tight')
print(f"Saved: {output_dir / 'time_analysis.png'}")
plt.close()

# ============================================================================
# Plot 6: Power Calculation (Voltage * Current)
# ============================================================================
df['Power_kW'] = (df['MCM DC Bus Voltage'] * df['MCM DC Bus Current']) / 1000

fig, ax = plt.subplots(figsize=fig_size)
ax.plot(df['Time'], df['Power_kW'], label='Calculated Power (V×I)', color='orange', linewidth=1.5)
ax.plot(df['Time'], df['pl_target'], label='PL Target Power', color='purple', linewidth=1.5, alpha=0.7, linestyle='--')
ax.set_xlabel('Time (s)')
ax.set_ylabel('Power (kW)')
ax.set_title('Calculated Power vs Power Limit Target')
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig(output_dir / 'power_comparison.png', dpi=300, bbox_inches='tight')
print(f"Saved: {output_dir / 'power_comparison.png'}")
plt.close()

# ============================================================================
# Plot 7: Lap Counter
# ============================================================================
fig, ax = plt.subplots(figsize=fig_size)
ax.plot(df['Time'], df['lap_counter'], label='Lap Counter', color='green', linewidth=1.5)
ax.set_xlabel('Time (s)')
ax.set_ylabel('Lap Number')
ax.set_title('Lap Counter Over Time')
ax.legend()
ax.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig(output_dir / 'lap_counter.png', dpi=300, bbox_inches='tight')
print(f"Saved: {output_dir / 'lap_counter.png'}")
plt.close()

# ============================================================================
# Plot 8: Comprehensive Overview (Multiple subplots)
# ============================================================================
fig, axes = plt.subplots(3, 2, figsize=(18, 12))
fig.suptitle('Comprehensive Data Overview', fontsize=18, fontweight='bold')

# Row 1: Motor metrics
axes[0, 0].plot(df['Time'], df['MCM Motor Speed'], label='Speed', color='green', linewidth=1)
axes[0, 0].set_ylabel('Speed (rpm)')
axes[0, 0].set_title('Motor Speed')
axes[0, 0].legend()
axes[0, 0].grid(True, alpha=0.3)

axes[0, 1].plot(df['Time'], df['MCM Torque Command'], label='Command', linewidth=1)
axes[0, 1].plot(df['Time'], df['MCM Torque Feedback'], label='Feedback', linewidth=1, alpha=0.7)
axes[0, 1].set_ylabel('Torque (NM)')
axes[0, 1].set_title('Torque')
axes[0, 1].legend()
axes[0, 1].grid(True, alpha=0.3)

# Row 2: Power and Energy
axes[1, 0].plot(df['Time'], df['Power_kW'], label='Power', color='orange', linewidth=1)
axes[1, 0].set_ylabel('Power (kW)')
axes[1, 0].set_title('Calculated Power')
axes[1, 0].legend()
axes[1, 0].grid(True, alpha=0.3)

axes[1, 1].plot(df['Time'], df['energy_consumption_per_lap'], label='Consumption', color='red', linewidth=1)
axes[1, 1].plot(df['Time'], df['energy_budget_per_lap'], label='Budget', color='blue', linewidth=1, alpha=0.7)
axes[1, 1].set_ylabel('Energy (kWh)')
axes[1, 1].set_title('Energy per Lap')
axes[1, 1].legend()
axes[1, 1].grid(True, alpha=0.3)

# Row 3: Throttle and Time
axes[2, 0].plot(df['Time'], df['TPS0ThrottlePercent0FF'], label='TPS0', linewidth=1)
axes[2, 0].plot(df['Time'], df['TPS1ThrottlePercent0FF'], label='TPS1', linewidth=1, alpha=0.7)
axes[2, 0].set_xlabel('Time (s)')
axes[2, 0].set_ylabel('Throttle (%)')
axes[2, 0].set_title('Throttle Position')
axes[2, 0].legend()
axes[2, 0].grid(True, alpha=0.3)

axes[2, 1].plot(df['Time'], df['time_in_straight'], label='Straights', color='blue', linewidth=1)
axes[2, 1].plot(df['Time'], df['time_in_corners'], label='Corners', color='red', linewidth=1)
axes[2, 1].set_xlabel('Time (s)')
axes[2, 1].set_ylabel('Time (s)')
axes[2, 1].set_title('Time Analysis')
axes[2, 1].legend()
axes[2, 1].grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig(output_dir / 'comprehensive_overview.png', dpi=300, bbox_inches='tight')
print(f"Saved: {output_dir / 'comprehensive_overview.png'}")
plt.close()

print(f"\nAll plots saved to: {output_dir}")
print("Generated plots:")
print("  1. motor_controller_metrics.png - Torque, Speed, Voltage, Current")
print("  2. throttle_position.png - TPS0 and TPS1 over time")
print("  3. energy_metrics.png - Energy consumption, budget, and carry over")
print("  4. power_limit_target.png - PL target power over time")
print("  5. time_analysis.png - Time in straights vs corners")
print("  6. power_comparison.png - Calculated power vs PL target")
print("  7. lap_counter.png - Lap counter over time")
print("  8. comprehensive_overview.png - All key metrics in one view")

