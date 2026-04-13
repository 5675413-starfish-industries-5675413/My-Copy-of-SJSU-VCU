"""
Mapping from DynamicConfig (struct_name, param_name) to HIL restbus transport actions.

Only parameters that must be delivered via CAN restbus simulation belong here.
Parameters injectable via HIL_CMD_INJECT are handled automatically by _lookup_param()
in send_data.py — no entry needed here for those.

To add a new restbus-simulated subsystem:
  1. Add its CAN handler class (like MCM) in the hil/core/ directory
  2. Add entries here mapping (struct_name, param_name) -> (handler_attr, method_name)
  3. Instantiate and wire up the handler in HILEnvironment.setup()
"""

HIL_RESTBUS_MAP: dict[tuple[str, str], tuple[str, str]] = {
    # -------------------------------------------------------------------------
    # MotorController — restbus simulation via DBC-encoded CAN frames
    # -------------------------------------------------------------------------
    ("MotorController", "motorRPM"):        ("_mcm", "set_rpm"),
    ("MotorController", "DC_Voltage"):      ("_mcm", "set_voltage"),
    ("MotorController", "DC_Current"):      ("_mcm", "set_current"),
    ("MotorController", "commandedTorque"): ("_mcm", "set_commanded_torque"),
    ("MotorController", "feedbackTorque"):  ("_mcm", "set_torque_feedback"),
}