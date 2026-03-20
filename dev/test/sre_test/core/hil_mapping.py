"""
Mapping from DynamicConfig (struct_name, param_name) to HIL transport actions.

Each entry maps:
    (struct_name, param_name) -> (transport, action)

  transport: "mcm"      — calls getattr(mcm, action)(value), then mcm.send_all()
             "injector" — calls injector.inject(action, value)
  action:    MCM method name, or 6-char injector parameter name

To add a new parameter:
  1. Add the C struct member (sre sil extract to refresh struct_members_output.json)
  2. (HIL) Add HIL_ADD_PARAM() entry in dev/hilParameter.c
  3. Append one row here — no other code changes needed.
"""

HIL_PARAM_MAP: dict[tuple[str, str], tuple[str, str]] = {
    # -------------------------------------------------------------------------
    # MotorController — restbus simulation via DBC-encoded CAN frames
    # -------------------------------------------------------------------------
    ("MotorController", "motorRPM"):        ("mcm", "set_rpm"),
    ("MotorController", "DC_Voltage"):      ("mcm", "set_voltage"),
    ("MotorController", "DC_Current"):      ("mcm", "set_current"),
    ("MotorController", "commandedTorque"): ("mcm", "set_commanded_torque"),
    ("MotorController", "feedbackTorque"):  ("mcm", "set_torque_feedback"),

    # -------------------------------------------------------------------------
    # PowerLimit — parameter injection via CAN 0x5FE
    # -------------------------------------------------------------------------
    ("PowerLimit", "plToggle"):                  ("injector", "pltog"),
    ("PowerLimit", "plStatus"):                  ("injector", "plstat"),
    ("PowerLimit", "plMode"):                    ("injector", "plmode"),
    ("PowerLimit", "plTargetPower"):             ("injector", "pltarg"),
    ("PowerLimit", "plKwLimit"):                 ("injector", "plkwlm"),
    ("PowerLimit", "plInitializationThreshold"): ("injector", "plinit"),
    ("PowerLimit", "plTorqueCommand"):           ("injector", "pltqcm"),
    ("PowerLimit", "clampingMethod"):            ("injector", "plclmp"),

    # -------------------------------------------------------------------------
    # Efficiency — parameter injection via CAN 0x5FE
    # -------------------------------------------------------------------------
    ("Efficiency", "efficiencyToggle"):    ("injector", "eftog"),
    ("Efficiency", "energyBudget_kWh"):    ("injector", "efbudg"),
    ("Efficiency", "finishedLap"):         ("injector", "effin"),
    ("Efficiency", "totalLapDistance_km"): ("injector", "efdist"),
}
