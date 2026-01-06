"""
CAN bus interface wrapper for SRE HIL platform.

This module provides a clean interface to python-can, using the
configuration from config/can.ini.
"""

import os
import configparser
from pathlib import Path
from typing import Optional

import can


class CANInterface:
    """
    Wrapper for python-can Bus interface.

    Reads configuration from config/can.ini automatically.
    Provides graceful error handling for missing hardware.
    """

    def __init__(self):
        """
        Initialize CAN bus interface.

        Raises:
            Exception: If CAN hardware is not available
        """
        self.bus = None
        self.channel_info = "Unknown"

        try:
            bus_kwargs = self._resolve_bus_kwargs()
            self.bus = can.interface.Bus(**bus_kwargs)
            self.channel_info = self._get_channel_info()
        except Exception as e:
            raise Exception(f"Failed to initialize CAN interface: {e}")

    @staticmethod
    def _default_config_path() -> Path:
        return Path(__file__).parent.parent.parent / "config" / "can.ini"

    def _resolve_bus_kwargs(self) -> dict:
        interface: Optional[str] = None
        channel: Optional[str] = None
        bitrate: Optional[int] = None

        env_interface = os.environ.get("SRE_HIL_CAN_INTERFACE")
        env_channel = os.environ.get("SRE_HIL_CAN_CHANNEL")
        env_bitrate = os.environ.get("SRE_HIL_CAN_BITRATE")

        if interface is None and env_interface:
            interface = env_interface
        if channel is None and env_channel:
            channel = env_channel
        if bitrate is None and env_bitrate:
            try:
                bitrate = int(env_bitrate)
            except ValueError:
                pass

        if interface is None or channel is None or bitrate is None:
            cfg_path = self._default_config_path()
            if cfg_path.exists():
                parser = configparser.ConfigParser()
                parser.read(cfg_path)
                section = parser["default"] if parser.has_section("default") else {}

                if interface is None:
                    interface = section.get("interface")
                if channel is None:
                    channel = section.get("channel")
                if bitrate is None:
                    try:
                        bitrate = int(section.get("bitrate", ""))
                    except (TypeError, ValueError):
                        bitrate = None

        bus_kwargs: dict = {}
        if interface is not None:
            bus_kwargs["interface"] = interface
        if channel is not None:
            bus_kwargs["channel"] = channel
        if bitrate is not None:
            # Virtual backend doesn't take bitrate.
            if interface is None or str(interface).lower() != "virtual":
                bus_kwargs["bitrate"] = bitrate

        if not bus_kwargs:
            bus_kwargs = {}

        return bus_kwargs

    def _get_channel_info(self) -> str:
        """
        Get human-readable channel information.

        Returns:
            String describing the CAN channel (e.g., "PCAN_USBBUS1 @ 500kbps")
        """
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
