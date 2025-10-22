"""
Plot Power Limit Test Results
==============================
Visualize test results from test_results_output.csv
"""

import pandas as pd
import matplotlib.pyplot as plt
import os

# Get script directory
script_dir = os.path.dirname(os.path.abspath(__file__))
csv_path = os.path.join(script_dir, "test_results_output.csv")

# Read CSV data
print(f"Reading data from: {csv_path}")
df = pd.read_csv(csv_path)

print(f"Loaded {len(df)} rows of data")
print(f"Columns: {list(df.columns)}")

# Create figure with subplots
fig, axes = plt.subplots(2, 2, figsize=(14, 10))
fig.suptitle('Power Limit Test Results Over Time', fontsize=16, fontweight='bold')

# Plot 1: TPS Input Over Time
ax1 = axes[0, 0]
ax1.plot(df['Row'], df['TPS_Input'], linewidth=1, color='blue', alpha=0.7)
ax1.set_xlabel('Row Number')
ax1.set_ylabel('TPS Input (%)')
ax1.set_title('TPS Input Over Time')
ax1.grid(True, alpha=0.3)

# Plot 2: Motor Speed Over Time
ax2 = axes[0, 1]
ax2.plot(df['Row'], df['Motor_Speed_Input'], linewidth=1, color='green', alpha=0.7)
ax2.set_xlabel('Row Number')
ax2.set_ylabel('Motor Speed (RPM)')
ax2.set_title('Motor Speed Over Time')
ax2.grid(True, alpha=0.3)

# Plot 3: Calculated Power Over Time
ax3 = axes[1, 0]
ax3.plot(df['Row'], df['Calculated_Power_kW'], linewidth=1, color='red', alpha=0.7)
ax3.set_xlabel('Row Number')
ax3.set_ylabel('Calculated Power (kW)')
ax3.set_title('Power Output Over Time')
ax3.grid(True, alpha=0.3)
ax3.axhline(y=40, color='green', linestyle='--', label='40kW Target', linewidth=2)
ax3.legend()

# Plot 4: PL Torque Command Over Time
ax4 = axes[1, 1]
ax4.plot(df['Row'], df['PL_TorqueCommand'], linewidth=1, color='orange', alpha=0.7)
ax4.set_xlabel('Row Number')
ax4.set_ylabel('PL Torque Command (Nm)')
ax4.set_title('PL Torque Command Over Time')
ax4.grid(True, alpha=0.3)

# Adjust layout to prevent overlap
plt.tight_layout()

# Save figure
output_path = os.path.join(script_dir, "test_results_plot.png")
plt.savefig(output_path, dpi=150, bbox_inches='tight')
print(f"\nPlot saved to: {output_path}")

# Show plot
plt.show()

# Print summary statistics
print("\n=== Summary Statistics ===")
print(f"Average Power: {df['Calculated_Power_kW'].mean():.2f} kW")
print(f"Max Power: {df['Calculated_Power_kW'].max():.2f} kW")
print(f"Min Power: {df['Calculated_Power_kW'].min():.2f} kW")
print(f"Average Torque Command: {df['PL_TorqueCommand'].mean():.2f} Nm")
print(f"PL Active {df['PL_Status'].sum()} / {len(df)} rows ({100*df['PL_Status'].mean():.1f}%)")

