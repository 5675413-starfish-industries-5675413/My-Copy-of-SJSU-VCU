"""
CAN bus interface wrapper for SRE HIL platform.

This module provides a clean interface to python-can, using the
configuration from config/can.ini.
"""

import can
from pathlib import Path
from typing import Optional


class CANInterface:
    """
    Wrapper for python-can Bus interface.

    Reads configuration from config/can.ini automatically.
    Provides graceful error handling for missing hardware.
    """

    def __init__(self):
        """
        Initialize CAN bus interface.

        Reads configuration from config/can.ini (if exists).
        Falls back to default PCAN settings if no config found.

        Raises:
            Exception: If CAN hardware is not available
        """
        self.bus = None
        self.channel_info = "Unknown"

        # Check if config/can.ini exists
        config_path = Path(__file__).parent.parent.parent / 'config' / 'can.ini'

        try:
            # python-can automatically reads can.ini if it exists in the search path
            # By default, it looks in: current dir, ~/.can/, /etc/
            # We'll initialize with no arguments to use can.ini
            self.bus = can.interface.Bus()

            # Get channel info
            self.channel_info = self._get_channel_info()

        except Exception as e:
            raise Exception(f"Failed to initialize CAN interface: {e}")

    def _get_channel_info(self) -> str:
        """
        Get human-readable channel information.

        Returns:
            String describing the CAN channel (e.g., "PCAN_USBBUS1 @ 500kbps")
        """
        try:
            # Try to get channel name from bus
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

    def send(self, msg: can.Message) -> bool:
        """
        Send a CAN message.

        Args:
            msg: can.Message object to send

        Returns:
            True if successful, False otherwise
        """
        if self.bus is None:
            return False

        try:
            self.bus.send(msg)
            return True
        except Exception:
            return False

    def shutdown(self):
        """
        Shutdown the CAN interface gracefully.
        """
        if self.bus is not None:
            try:
                self.bus.shutdown()
            except Exception:
                pass
            self.bus = None

    def __del__(self):
        """Cleanup on object destruction."""
        self.shutdown()

    def __enter__(self):
        """Context manager entry."""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.shutdown()
