class MCM:
    MSG_POSITION = "MCM_Motor_Position_Info"
    MSG_VOLTAGE = "MCM_Voltage_Info"
    MSG_CURRENT = "MCM_Current_Info"

    def __init__(self, dbc, can_interface):
        self.dbc = dbc
        self.can = can_interface

        self._rpm = 0
        self._dc_voltage = 0.0
        self._dc_current = 0.0

    def set_rpm(self, rpm: int):
        self._rpm = rpm
    
    def set_voltage(self, voltage: float):
        self._dc_voltage = voltage
    
    def set_current(self, current: float):
        self._dc_current = current

    def send_all(self):
        # Send position info
        msg = self.dbc.encode_message(self.MSG_POSITION, {"MCM_Motor_Speed": self._rpm})
        self.can.send(msg)

        # Send voltage info
        msg = self.dbc.encode_message(self.MSG_VOLTAGE, {"MCM_DC_Bus_Voltage": self._dc_voltage})
        self.can.send(msg)

        # Send current info
        msg = self.dbc.encode_message(self.MSG_CURRENT, {"MCM_DC_Bus_Current": self._dc_current})
        self.can.send(msg)
    