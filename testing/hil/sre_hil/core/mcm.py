from dataclasses import dataclass

@dataclass
class MCMState:
    rpm: int = 0
    motor_angle: float = 0.0
    dc_voltage: float = 0.0
    dc_current: float = 0.0
    commanded_torque: float = 0.0
    torque_feedback: float = 0.0

class MCM:
    MSG_POSITION = "MCM_Motor_Position_Info"
    MSG_VOLTAGE = "MCM_Voltage_Info"
    MSG_CURRENT = "MCM_Current_Info"
    MSG_TORQUE_TIMER = "MCM_Torque_And_Timer_Info"

    def __init__(self, dbc, can_interface):
        self.dbc = dbc
        self.can = can_interface
        self.state = MCMState()
        
    def set_rpm(self, rpm: int):
        self.state.rpm = rpm
    
    def set_voltage(self, voltage: float):
        self.state.dc_voltage = voltage
    
    def set_current(self, current: float):
        self.state.dc_current = current

    def set_dc_bus(self, voltage: float, current: float):
        self.state.dc_voltage = voltage
        self.state.dc_current = current

    def set_commanded_torque(self, torque: float):
        self.state.commanded_torque = torque
    
    def set_torque_feedback(self, torque: float):
        self.state.torque_feedback = torque
    
    def set_motor_angle(self, angle: float):
        self.state.motor_angle = angle

    def set_power_kw(self, power_kw: float, voltage: float = 350.0):
        self.state.dc_voltage = voltage
        self.state.dc_current = (power_kw * 1000.0) / voltage

    @staticmethod
    def torque_nm_from_power_kw(power_kw: float, rpm: int) -> float:
        """Convert mechanical power to torque using $T = P * 9549 / RPM$.

        Returns 0.0 if rpm <= 0.
        """
        if rpm <= 0:
            return 0.0
        return (power_kw * 9549.0) / float(rpm)

    @staticmethod
    def power_kw_from_torque_nm(torque_nm: float, rpm: int) -> float:
        """Convert torque to power using $P = T * RPM / 9549$.

        Returns 0.0 if rpm <= 0.
        """
        if rpm <= 0:
            return 0.0
        return (float(torque_nm) * float(rpm)) / 9549.0
    
    def get_power_kw(self) -> float:
        return (self.state.dc_voltage * self.state.dc_current) / 1000.0

    def set_pl_test_conditions(self, rpm: int, power_kw: float, 
                                commanded_torque: float = None,
                                voltage: float = 350.0):
        self.state.rpm = rpm
        self.set_power_kw(power_kw, voltage)
        
        if commanded_torque is None and rpm > 0:
            self.state.commanded_torque = self.torque_nm_from_power_kw(power_kw, rpm)
        elif commanded_torque is not None:
            self.state.commanded_torque = commanded_torque

    def set_pl_overpower_conditions(
        self,
        rpm: int,
        target_power_kw: float,
        overpower_kw: float,
        voltage: float = 350.0,
        commanded_torque: float | None = None,
    ):
        """Set MCM signals to represent a power draw above a target.

        This is a convenience wrapper for PL tests: it sets DC bus signals to
        `target_power_kw + overpower_kw` and sets commanded torque (defaults to
        torque equivalent of that power at the given RPM).
        """
        power_kw = float(target_power_kw) + float(overpower_kw)
        self.set_pl_test_conditions(
            rpm=rpm,
            power_kw=power_kw,
            voltage=voltage,
            commanded_torque=commanded_torque,
        )

    def send_all(self):
        self._send_position()
        self._send_voltage()
        self._send_current()
        self._send_torque_timer()
    
    def _send_position(self):
        msg = self.dbc.encode_message(self.MSG_POSITION, {
            "MCM_Motor_Speed": self.state.rpm,
            "MCM_Motor_Angle": self.state.motor_angle,
            "MCM_Electrical_Output_Freq": 0,
            "MCM_Resolver_Angle": 0
        })
        self.can.send(msg)

    def _send_voltage(self):
        msg = self.dbc.encode_message(self.MSG_VOLTAGE, {
            "MCM_DC_Bus_Voltage": self.state.dc_voltage,
            "MCM_Output_Voltage": 0,
            "MCM_Phase_AB_Voltage": 0,
            "MCM_Phase_BC_Voltage": 0
        })
        self.can.send(msg)

    def _send_current(self):
        msg = self.dbc.encode_message(self.MSG_CURRENT, {
            "MCM_DC_Bus_Current": self.state.dc_current,
            "MCM_Phase_A_Current": 0,
            "MCM_Phase_B_Current": 0,
            "MCM_Phase_C_Current": 0
        })
        self.can.send(msg)
    
    def _send_torque_timer(self):
        msg = self.dbc.encode_message(self.MSG_TORQUE_TIMER, {
            "MCM_Commanded_Torque": self.state.commanded_torque,
            "MCM_Torque_Feedback": self.state.torque_feedback,
            "MCM_Power_On_Timer": 0
        })
        self.can.send(msg)
