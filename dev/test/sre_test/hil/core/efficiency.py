from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING
import time
import struct

if TYPE_CHECKING:
    from .param_injector import HILParamInjector

@dataclass
class EfficiencyStatus:
    toggle: int = 0
    energy_budget_kwh: float = 0.0
    finished_lap: bool = False
    lap_counter: int = 0
    energySpentInCorners_kWh: float = 0.0
    lapEnergySpent_kWh: float = 0.0
    totalLapDistance_km: float = 0.0

def _f32_bits(x: float) -> int:
    """Float -> int32 bits for TYPE_FLOAT4 injection."""
    return struct.unpack("<i", struct.pack("<f", float(x)))[0]


class EfficiencyController:
    def __init__(self, injector: "HILParamInjector"):
        self.injector = injector
        self._pending: dict[str, int | float] = {}

    def set_toggle(self, enabled: bool):
        self._pending["eftog"] = int(enabled)

    def set_energy_budget_kwh(self, kwh: float):
        self._pending["efbudg"] = kwh

   # def set_carry_over_energy_kwh(self, kwh: float):
       # self._pending["efcary"] = _f32_bits(kwh)

    def set_finished_lap(self, done: bool):
        self._pending["effin"] = int(done)

    def set_total_lap_distance_km(self, km: float):
        self._pending["efdist"] = km

    def send_all(self):
        """Sends all pending parameters to VCU via injector"""
        for name, value in self._pending.items():
            self.injector.inject(name, value)
        self._pending.clear()

class EfficiencyDecoder:
    MSG_EFF_SETTINGS = "VCU_Efficiency_Settings"

    def __init__(self, dbc):
        self.dbc = dbc
        self._status = EfficiencyStatus()

        self._id_eff_settings = self.dbc.get_message(self.MSG_EFF_SETTINGS).frame_id
    
    def is_eff_message(self, msg) -> bool:
        if msg is None:
            return False
        return msg.arbitration_id == self._id_eff_settings
    
    def decode(self, msg) -> EfficiencyStatus:
        if msg is None:
            return self._status
        
        decoded = self.dbc.decode_message(msg)
        signals = decoded['signals']

        if msg.arbitration_id == self._id_eff_settings:
            self._status.lap_counter = int(signals.get(
                "VCU_Efficiency_LapCounter", 0))
            self._status.energySpentInCorners_kWh = int(signals.get(
                "VCU_Efficiency_EnergySpentInCorners", 0))
            self._status.lapEnergySpent_kWh = int(signals.get(
                "VCU_Efficiency_LapEnergySpent", 0))
            # The CAN signal is scaled by 100 in canManager.c, so we divide by 100.0 to get km.
            self._status.totalLapDistance_km = float(signals.get(
                "VCU_Efficiency_TotalLapDistance", 0)) / 100.0
            
        return self._status
    
    def get_status(self) -> EfficiencyStatus:
        return self._status
    
    def reset(self):
        self._status = EfficiencyStatus()


class EfficiencyMonitor:
    def __init__(self, dbc, can_interface):
        self.can = can_interface
        self.decoder = EfficiencyDecoder(dbc)

    def recv(self, timeout: float = 0.0):
        """Receive one frame and decode if it is Efficiency message."""
        msg = self.can.receive(timeout=timeout)
        if not self.decoder.is_eff_message(msg):
            return None
        return self.decoder.decode(msg)

    def wait_for_status(self, timeout_s: float = 1.0, poll_timeout_s: float = 0.01):
        """Wait for any Efficiency status frame or timeout."""
        t_end = time.time() + timeout_s
        while time.time() < t_end:
            status = self.recv(timeout=poll_timeout_s)
            if status is not None:
                return status
        return None