from enum import Enum

class HILState(Enum):
    DISCONNECTED = 0        # HIL not enabled
    INIT_PENDING = 1        # HIL init command sent, waiting for VCU to enter DIAGNOSTIC
    DIAGNOSTIC = 2          # Collecting + validating struct param counts from VCU
    READY = 3               # HIL session active, ready for inject or request operations
    INJECT_PENDING = 4      # Injection frame sent, waiting for ACK
    REQUEST_PENDING = 5     # Output request sent, waiting for response
    ERROR = 6               # Error state

class HILStateMachine:
    pass