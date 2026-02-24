from dataclasses import dataclass
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .param_injector import HILParamInjector

@dataclass
class PowerLimitStatus:
    initialization_threshold: int = 0
    mode: int = 0
    status: bool = False
    pid_total_error: int = 0
    pid_proportional: int = 0
    pid_integral: int = 0
    torque_command: float = 0.0
    target_power: int = 0
    pid_output: int = 0
    clamping_method: int = 0

# Decoder for PowerLimit CAN messages from DBC to VCU
class PowerLimitDecoder:
    MSG_STATUS_A = "VCU_Power_Limit_Status_AMsg"
    MSG_STATUS_B = "VCU_Power_Limit_Status_BMsg"
    
    def __init__(self, dbc):
        self.dbc = dbc
        self._status = PowerLimitStatus()
        
        self._id_status_a = dbc.get_message(self.MSG_STATUS_A).frame_id
        self._id_status_b = dbc.get_message(self.MSG_STATUS_B).frame_id
    
    def is_pl_message(self, msg) -> bool:
        if msg is None:
            return False
        return msg.arbitration_id in (self._id_status_a, self._id_status_b)
    
    def decode(self, msg) -> PowerLimitStatus:
        if msg is None:
            return self._status
            
        decoded = self.dbc.decode_message(msg)
        signals = decoded['signals']
        
        if msg.arbitration_id == self._id_status_a:
            self._status.initialization_threshold = int(signals.get(
                "VCU_POWERLIMIT_getInitialisationThreshold_W", 0))
            self._status.mode = int(signals.get(
                "VCU_POWERLIMIT_getMode_W", 0))
            self._status.status = bool(signals.get(
                "VCU_POWERLIMIT_getStatus_W", 0))
            self._status.pid_total_error = int(signals.get(
                "VCU_POWERLIMIT_PID_getTotalError",
                signals.get("VCU_PID_getTotalError", 0),
            ))
            self._status.pid_proportional = int(signals.get(
                "VCU_POWERLIMIT_PID_getProportional",
                signals.get("VCU_PID_getProportional", 0),
            ))
                
        elif msg.arbitration_id == self._id_status_b:
            self._status.pid_integral = int(signals.get(
                "VCU_POWERLIMIT_PID_getIntegral",
                signals.get("VCU_PID_getIntegral", 0),
            ))
            self._status.torque_command = float(signals.get(
                "VCU_POWERLIMIT_getTorqueCommand_Nm", 0))
            self._status.target_power = int(signals.get(
                "VCU_POWERLIMIT_getTargetPower_W", 0))
            self._status.pid_output = int(signals.get(
                "VCU_POWERLIMIT_PID_getOutput",
                signals.get("VCU_PID_getOutput", 0),
            ))
            self._status.clamping_method = int(signals.get(
                "VCU_POWERLIMIT_getClampingMethod_W", 0))
        
        return self._status
    
    def get_status(self) -> PowerLimitStatus:
        return self._status
    
    def reset(self):
        self._status = PowerLimitStatus()

# CAN monitoring handler for PowerLimit messages
class PowerLimitMonitor:
    """Receives and decodes Power Limit status frames."""

    def __init__(self, dbc, can_interface):
        self.can = can_interface
        self.decoder = PowerLimitDecoder(dbc)

    def recv(self, timeout: float = 0.0):
        """Receive one frame and decode if it is PL."""
        msg = self.can.receive(timeout=timeout)
        if not self.decoder.is_pl_message(msg):
            return None
        return self.decoder.decode(msg)

    def wait_for_status(self, timeout_s: float = 1.0, poll_timeout_s: float = 0.01):
        """Wait for any PL status frame or timeout."""
        import time

        t_end = time.time() + timeout_s
        while time.time() < t_end:
            status = self.recv(timeout=poll_timeout_s)
            if status is not None:
                return status
        return None

class PowerLimitController:
    def __init__(self, injector: "HILParamInjector"):
        self.injector = injector
        self._pending: dict[str, int] = {}

    def set_toggle(self, enabled: bool):
        self._pending['pltog'] = int(enabled)

    def set_status(self, status: bool):
        self._pending['plstat'] = int(status)
    
    def set_mode(self, mode: int):
        self._pending['plmode'] = mode

    def set_target_power(self, kw: int):
        self._pending['pltarg'] = kw

    def set_kw_limit(self, kw_limit: int):
        self._pending['plkwlm'] = kw_limit

    def set_initialization_threshold(self, threshold: int):
        self._pending['plinit'] = threshold

    def set_torque_command(self, torque: int):
        self._pending['pltqcm'] = torque

    def set_clamping_method(self, method: int):
        self._pending['plclmp'] = method
    
    def send_all(self):
        """Sends all pending parameters to VCU via injector"""
        for name, value in self._pending.items():
            self.injector.inject(name, value)
        self._pending.clear()