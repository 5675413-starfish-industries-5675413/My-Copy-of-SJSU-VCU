"""
HIL backend implementation of TestEnvironment.
Translates DynamicConfig inputs to CAN bus actions (MCM restbus + injector),
and polls monitors for the canonical response dict.
"""
from dataclasses import asdict
from typing import Dict, Any, Optional

from sre_test.core.environment import TestEnvironment
from sre_test.core.config import DynamicConfig
from sre_test.core.hil_mapping import HIL_PARAM_MAP

from sre_test.hil.core.can_interface import CANInterface
from sre_test.hil.core.dbc_loader import DBCLoader
from sre_test.hil.core.param_injector import HILParamInjector
from sre_test.hil.core.mcm import MCM
from sre_test.hil.core.power_limit import PowerLimitMonitor
from sre_test.hil.core.efficiency import EfficiencyMonitor


class HILEnvironment(TestEnvironment):
    """TestEnvironment backed by real VCU hardware over CAN bus."""

    def __init__(self, dbc_path: Optional[str] = None):
        self._dbc_path = dbc_path
        self._can: Optional[CANInterface] = None
        self._dbc: Optional[DBCLoader] = None
        self._injector: Optional[HILParamInjector] = None
        self._mcm: Optional[MCM] = None
        self._pl_monitor: Optional[PowerLimitMonitor] = None
        self._eff_monitor: Optional[EfficiencyMonitor] = None

    def setup(self) -> None:
        self._can = CANInterface()
        self._dbc = DBCLoader(self._dbc_path)
        self._injector = HILParamInjector(self._can)
        self._mcm = MCM(self._dbc, self._can)
        self._pl_monitor = PowerLimitMonitor(self._dbc, self._can)
        self._eff_monitor = EfficiencyMonitor(self._dbc, self._can)

    def teardown(self) -> None:
        if self._can is not None:
            self._can.shutdown()
            self._can = None

    def send_inputs(self, *configs: DynamicConfig) -> None:
        mcm_dirty = False

        for config in configs:
            struct_name = config.struct_name
            for param_name, value in config._values.items():
                if value is None:
                    continue

                key = (struct_name, param_name)
                if key not in HIL_PARAM_MAP:
                    raise KeyError(
                        f"No HIL mapping for {struct_name}.{param_name}. "
                        f"Add an entry to sre_test/core/hil_mapping.py."
                    )

                transport, action = HIL_PARAM_MAP[key]

                if transport == "mcm":
                    getattr(self._mcm, action)(value)
                    mcm_dirty = True
                elif transport == "injector":
                    self._injector.inject(action, value)

        # Flush all four MCM CAN frames in one batch if any MCM param was set
        if mcm_dirty:
            self._mcm.send_all()

    def receive_outputs(self, timeout: float = 2.0) -> Dict[str, Any]:
        """
        Poll HIL monitors and return the canonical response dict:
            {"power_limit": {...}, "efficiency": {...}}
        Subsystems that don't respond within timeout are omitted.
        """
        result: Dict[str, Any] = {}

        pl_status = self._pl_monitor.wait_for_status(timeout_s=timeout)
        if pl_status is not None:
            result["power_limit"] = asdict(pl_status)

        eff_status = self._eff_monitor.wait_for_status(timeout_s=timeout)
        if eff_status is not None:
            result["efficiency"] = asdict(eff_status)

        return result
