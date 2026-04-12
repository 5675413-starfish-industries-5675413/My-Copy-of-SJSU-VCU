"""
CAN bus interface wrapper for SRE HIL platform.

This module provides a clean interface to python-can.
Configuration is resolved via python-can's native load_config(), which checks
(in order): can.rc, CAN_INTERFACE/CAN_CHANNEL/CAN_BITRATE env vars, and
~/can.conf (valid on all platforms). On first use, ~/can.conf is created
automatically from the project template if it does not already exist.
"""

import shutil
from pathlib import Path
from typing import Optional

import can
import can.util

_CONFIG_TEMPLATE = Path(__file__).resolve().parents[2] / "config" / "can.ini"
_CONFIG_TARGET   = Path.home() / "can.conf"


class CANInterface:
    """
    Wrapper for python-can Bus interface.

    On first use, creates ~/can.conf from the project template if it does not
    exist. Configuration is then resolved via python-can's native config chain.
    Provides graceful error handling for missing hardware.
    """

    def __init__(self):
        """
        Initialize CAN bus interface.

        Raises:
            Exception: If CAN hardware is not available or config is missing.
        """
        self.bus = None
        self.channel_info = "Unknown"

        self._ensure_config()

        try:
            bus_kwargs = self._resolve_bus_kwargs()
            self.bus = can.interface.Bus(**bus_kwargs)
            self.channel_info = self._get_channel_info()
        except Exception as e:
            raise Exception(f"Failed to initialize CAN interface: {e}")

    @staticmethod
    def _ensure_config():
        if _CONFIG_TARGET.exists():
            return
        if _CONFIG_TEMPLATE.exists():
            shutil.copy(_CONFIG_TEMPLATE, _CONFIG_TARGET)
            print(
                f"[CANInterface] Created {_CONFIG_TARGET} from project template. "
                "Edit this file if your hardware uses a different interface or channel."
            )

    def _resolve_bus_kwargs(self) -> dict:
        config = dict(can.util.load_config())
        # virtual backend does not accept bitrate
        if str(config.get("interface", "")).lower() == "virtual":
            config.pop("bitrate", None)
        return config

    def _get_channel_info(self) -> str:
        try:
            if hasattr(self.bus, 'channel_info'):
                return self.bus.channel_info
            elif hasattr(self.bus, '_channel'):
                return str(self.bus._channel)
            else:
                return f"{self.bus.__class__.__name__}"
        except:
            return "CAN Bus"

    def receive(self, timeout: Optional[float] = None) -> Optional[can.Message]:
        """
        Receive a CAN message.

        Args:
            timeout: Timeout in seconds (None = blocking, 0 = non-blocking)

        Returns:
            can.Message if received, None if timeout
        """
        if self.bus is None:
            return None

        try:
            return self.bus.recv(timeout=timeout)
        except Exception:
            return None

    def send(self, arbitration_id: int, data: bytes, is_extended_id: bool = False) -> bool:
        """
        Send a CAN message.

        Args:
            arbitration_id: CAN frame ID
            data: Frame payload bytes
            is_extended_id: Whether to use 29-bit extended ID

        Returns:
            True if successful, False otherwise
        """
        if self.bus is None:
            return False

        try:
            msg = can.Message(
                arbitration_id=arbitration_id,
                data=data,
                is_extended_id=is_extended_id,
            )
            self.bus.send(msg)
            return True
        except Exception:
            return False

    def shutdown(self):
        """Shutdown the CAN interface gracefully."""
        if self.bus is not None:
            try:
                self.bus.shutdown()
            except Exception:
                pass
            self.bus = None

    def __del__(self):
        self.shutdown()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.shutdown()
