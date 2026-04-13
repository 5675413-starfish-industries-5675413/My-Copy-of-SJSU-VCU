"""
HIL backend implementation of TestEnvironment.
Translates DynamicConfig inputs to CAN bus actions (MCM restbus + injector),
and polls monitors for the canonical response dict.
"""
from dataclasses import asdict
from typing import Dict, Any, Optional

from sre_test.core.environment import TestEnvironment
from sre_test.core.config import DynamicConfig, configs_to_json
from sre_test.core.hil_mapping import HIL_RESTBUS_MAP

from sre_test.hil.core.can_interface import CANInterface
from sre_test.hil.core.dbc_loader import DBCLoader

from sre_test.hil.core.hil_state_machine import HILStateMachine
from sre_test.hil.core.send_data import HILSendHandler, _lookup_param
from sre_test.hil.core.receive_data import HILReceiveHandler, DIAG_TIMEOUT_S, ACK_TIMEOUT_S, VALUE_TIMEOUT_S

## OLD IMPORTS ##
# from sre_test.hil.core.param_injector import HILParamInjector
from sre_test.hil.core.mcm import MCM
# from sre_test.hil.core.power_limit import PowerLimitMonitor
# from sre_test.hil.core.efficiency import EfficiencyMonitor


class HILEnvironment(TestEnvironment):
    """TestEnvironment backed by real VCU hardware over CAN bus."""

    def __init__(self, dbc_path: Optional[str] = None):
        self._dbc_path = dbc_path
        self._can: Optional[CANInterface] = None
        self._dbc: Optional[DBCLoader] = None
        self._mcm: Optional[MCM] = None     # Restbus MCM object
        self._state_machine: Optional[HILStateMachine] = None
        self._send_handler: Optional[HILSendHandler] = None
        self._receive_handler: Optional[HILReceiveHandler] = None

    def setup(self) -> None:
        self._can = CANInterface()
        self._dbc = DBCLoader(self._dbc_path)
        self._mcm = MCM(self._can, self._dbc)
        self._state_machine = HILStateMachine(self._can)
        self._send_handler = HILSendHandler(self._can, self._state_machine)
        self._receive_handler = HILReceiveHandler(self._can, self._state_machine)
        self._send_handler.send_init()                        # Transition to INIT_PENDING and kick off the DIAG sequence
        self._receive_handler.listen(DIAG_TIMEOUT_S)         # INIT_PENDING → DIAGNOSTIC → READY

    def teardown(self) -> None:
        if self._can is not None:
            self._can.shutdown()
            self._can = None

    def send_inputs(self, *configs: DynamicConfig) -> None:
        configs_to_json(*configs)  # write active params to shared vcu_structs_map.json
        restbus_dirty: set[str] = set()   # Track which restbus subsystems we touched, to optimize send_all calls

        for config in configs:
            struct_name = config.struct_name
            for param_name, value in config._values.items():
                if value is None:
                    continue

                key = (struct_name, param_name)
                if key in HIL_RESTBUS_MAP:
                    handler_attr, method_name = HIL_RESTBUS_MAP[key]
                    getattr(getattr(self, handler_attr), method_name)(value)
                    restbus_dirty.add(handler_attr)
                else:
                    identifier, param_number = _lookup_param(struct_name, param_name)
                    self._send_handler.send_inject(identifier, param_number, value)
                    self._receive_handler.listen(ACK_TIMEOUT_S)
        
        for handler_attr in restbus_dirty:
            getattr(self, handler_attr).send_all()   # Flush all frames for touched restbus subsystems

    def receive_outputs(self, *configs: DynamicConfig) -> Dict[str, Any]:
        result: Dict[str, Any] = {}

        for config in configs:
            struct_result = {}
            for param_name in config._values:
                identifier, param_number = _lookup_param(config.struct_name, param_name)
                self._send_handler.send_request(identifier, param_number)
                value = self._receive_handler.listen(VALUE_TIMEOUT_S)
                if value is not None:
                    struct_result[param_name] = value
            if struct_result:
                result[config.struct_name] = struct_result

        return result