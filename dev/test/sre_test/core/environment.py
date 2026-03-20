from abc import ABC, abstractmethod
from typing import Dict, Any
from sre_test.core.config import DynamicConfig


class TestEnvironment(ABC):
    """Abstract base class for a test environment (SIL or HIL)."""

    @abstractmethod
    def setup(self) -> None:
        """Initialize the backend (compile binary, connect CAN, etc.)."""
        ...

    def teardown(self) -> None:
        """Shutdown and cleanup."""
        pass

    @abstractmethod
    def send_inputs(self, *configs: DynamicConfig) -> None:
        """Send DynamicConfig inputs to the target."""
        ...

    @abstractmethod
    def receive_outputs(self, timeout: float = 2.0) -> Dict[str, Any]:
        """Receive outputs from the target. Returns canonical response dict."""
        ...

    def __enter__(self):
        self.setup()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.teardown()
