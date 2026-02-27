"""
SIL backend implementation of TestEnvironment.
Wraps SILSimulator to provide the unified send_inputs / receive_outputs interface.
"""
from typing import Dict, Any, Optional

from sre_test.core.environment import TestEnvironment
from sre_test.core.config import DynamicConfig, configs_to_json
from sre_test.core.response import normalize_response, SIL_KEY_ALIASES
from sre_test.sil.core.simulator import SILSimulator


class SILEnvironment(TestEnvironment):
    """TestEnvironment backed by the compiled SIL binary (JSON IPC)."""

    def __init__(self, auto_compile: bool = True, verbose: bool = False):
        self._auto_compile = auto_compile
        self._verbose = verbose
        self._sim: Optional[SILSimulator] = None

    def setup(self) -> None:
        self._sim = SILSimulator.create(
            auto_compile=self._auto_compile,
            verbose=self._verbose,
        )

    def teardown(self) -> None:
        if self._sim is not None:
            self._sim.stop()
            self._sim = None

    def send_inputs(self, *configs: DynamicConfig) -> None:
        # 1. Write config values into struct_members_output.json
        configs_to_json(*configs)
        # 2. Send the named structs to the binary via stdin
        struct_names = tuple(c.struct_name for c in configs)
        self._sim.send_structs(*struct_names)

    def receive_outputs(self, timeout: float = 2.0) -> Dict[str, Any]:
        raw = self._sim.receive(timeout=timeout)
        if raw is None:
            return {}
        return normalize_response(raw, SIL_KEY_ALIASES)
