"""
CSV-to-struct parameter mappings used by SIL efficiency CLI commands.
"""

# CSV column -> MotorController parameter
CSV_TO_MCM_PARAMS = {
    "MCM DC Bus Voltage": "DC_Voltage",
    "MCM DC Bus Current": "DC_Current",
    "MCM Motor Speed": "motorRPM",
    "MCM Torque Command": "commandedTorque",
}

# CSV column -> TorqueEncoder parameter
CSV_TO_TPS_PARAMS = {
    "TPS0ThrottlePercent0FF": "tps0_percent",
    "TPS1ThrottlePercent0FF": "tps1_percent",
}


