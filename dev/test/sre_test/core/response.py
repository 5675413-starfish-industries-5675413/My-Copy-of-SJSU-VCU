"""
Canonical response schema and normalization for the unified TestEnvironment.

Both SILEnvironment and HILEnvironment must return receive_outputs() in this shape:

{
    "power_limit": {
        "status": bool,
        "mode": int,
        "target_power": int,           # Watts (from SIL) or raw int (from HIL)
        "torque_command": float,        # Nm
        "initialization_threshold": int,
        "clamping_method": int,
        # HIL-only (PID internals, present when HIL decodes Status A/B messages):
        "pid_total_error": int,
        "pid_proportional": int,
        "pid_integral": int,
        "pid_output": int,
    },
    "efficiency": {
        "lap_counter": int,
        "energy_budget_kwh": float,
        "total_lap_distance_km": float,
        # SIL-only (computed by firmware, not broadcast on CAN):
        "time_in_straight": float,
        "time_in_corners": float,
        "energy_consumption_per_lap": float,
        "carry_over_energy": float,
        # HIL-only (decoded from VCU_Efficiency_Settings):
        "energySpentInCorners_kWh": float,
        "lapEnergySpent_kWh": float,
        "totalLapDistance_km": float,
    },
    "motor_controller": {               # SIL-only section
        "motor_rpm": float,
        "dc_voltage": float,
        "dc_current": float,
        "power_kw": float,
    },
}

Notes:
- Subsystems absent from the response are simply omitted (not present as empty dicts).
- Fields marked "SIL-only" or "HIL-only" are present only on that backend.
  Backend-agnostic tests should only assert on fields present in both.
- SIL raw JSON uses prefixed keys (pl_status, pl_mode, ...).
  SIL_KEY_ALIASES maps these to the canonical names above.
- HIL uses dataclasses.asdict() which already produces canonical field names.
"""

from typing import Dict, Any

# Map SIL firmware JSON keys -> canonical names.
# Derived directly from dev/sil.c printf() calls.
SIL_KEY_ALIASES: Dict[str, str] = {
    # power_limit section
    "pl_status":                  "status",
    "pl_mode":                    "mode",
    "pl_target_power":            "target_power",
    "pl_initialization_threshold": "initialization_threshold",
    "pl_torque_command":          "torque_command",
    "pl_clamping_method":         "clamping_method",
    # efficiency section
    "energy_budget_per_lap":      "energy_budget_kwh",
    "total_lap_distance":         "total_lap_distance_km",
}


def normalize_response(raw: Dict[str, Any], aliases: Dict[str, str]) -> Dict[str, Any]:
    """
    Recursively rename keys in a nested dict according to aliases.
    Keys not present in aliases pass through unchanged.
    """
    result: Dict[str, Any] = {}
    for key, value in raw.items():
        canonical_key = aliases.get(key, key)
        if isinstance(value, dict):
            result[canonical_key] = normalize_response(value, aliases)
        else:
            result[canonical_key] = value
    return result
