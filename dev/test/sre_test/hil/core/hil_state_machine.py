"""
HIL State machine for VCU testing

Tracks session state and performs state transitions 
"""

import time
from enum import Enum
from typing import Optional

class HILState(Enum):
    DISCONNECTED = 0        # HIL not enabled
    INIT_PENDING = 1        # HIL init command sent, waiting for VCU to enter DIAGNOSTIC
    DIAGNOSTIC = 2          # Collecting + validating struct param counts from VCU
    READY = 3               # HIL session active, ready for inject or request operations
    INJECT_PENDING = 4      # Injection frame sent, waiting for ACK
    REQUEST_PENDING = 5     # Output request sent, waiting for response
    ERROR = 6               # Error state

_VALID_TRANSITIONS: dict[HILState, set[HILState]] = {
    HILState.DISCONNECTED:      {HILState.INIT_PENDING},
    HILState.INIT_PENDING:      {HILState.DIAGNOSTIC, HILState.ERROR},
    HILState.DIAGNOSTIC:        {HILState.READY, HILState.ERROR},
    HILState.READY:             {HILState.INJECT_PENDING, HILState.REQUEST_PENDING},
    HILState.INJECT_PENDING:    {HILState.READY, HILState.ERROR},
    HILState.REQUEST_PENDING:   {HILState.READY, HILState.ERROR},
    HILState.ERROR:             set(),
}

_PENDING_STATES = {HILState.INIT_PENDING, HILState.INJECT_PENDING, HILState.REQUEST_PENDING}

class HILStateMachine:
    def __init__(self):
        self._state: HILState = HILState.DISCONNECTED
        self._pending_since: Optional[float] = None

    @property
    def state(self) -> HILState:
        return self._state
    
    def transition(self, new_state: HILState) -> None:
        if new_state not in _VALID_TRANSITIONS[self._state]:
            raise ValueError(
                f"Invalid transition: {self._state.name} -> {new_state.name}"
            )
        self._state = new_state
        self._pending_since = time.monotonic() if new_state in _PENDING_STATES else None
    
    def reset(self) -> None:
        self._state = HILState.DISCONNECTED
        self._pending_since = None

    def is_timed_out(self, timeout_s: float) -> bool:
        if self._pending_since is None:
            return False
        return time.monotonic() - self._pending_since > timeout_s