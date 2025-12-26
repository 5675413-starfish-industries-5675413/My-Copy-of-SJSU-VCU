"""
DBC file loader and message decoder for SRE HIL platform.

This module handles loading and parsing DBC files using cantools,
providing message encoding/decoding capabilities.
"""

import cantools
from pathlib import Path
from typing import Optional, Dict, Any
import can


class DBCLoader:
    """
    DBC file loader and message codec.

    Loads DBC files and provides message encoding/decoding functionality.
    Automatically searches for DBC files in the dbcs/ directory.
    """

    def __init__(self, dbc_path: Optional[str] = None):
        """
        Initialize DBC loader.

        Args:
            dbc_path: Path to DBC file. If None, uses default from dbcs/ directory.

        Raises:
            FileNotFoundError: If DBC file not found
            Exception: If DBC file cannot be parsed
        """
        self.dbc = None
        self.dbc_path = None
        self.dbc_name = None

        # Determine DBC path
        if dbc_path:
            self.dbc_path = Path(dbc_path)
        else:
            # Default: look for most recent DBC in dbcs/ directory
            dbcs_dir = Path(__file__).parent.parent.parent / 'dbcs'
            if dbcs_dir.exists():
                dbc_files = list(dbcs_dir.glob('*.dbc'))
                if dbc_files:
                    # Use the most recently modified DBC file
                    self.dbc_path = max(dbc_files, key=lambda p: p.stat().st_mtime)
                else:
                    raise FileNotFoundError(f"No DBC files found in {dbcs_dir}")
            else:
                raise FileNotFoundError(f"dbcs/ directory not found")

        # Validate file exists
        if not self.dbc_path.exists():
            raise FileNotFoundError(f"DBC file not found: {self.dbc_path}")

        # Load DBC file
        try:
            self.dbc = cantools.database.load_file(str(self.dbc_path), strict=False)
            self.dbc_name = self.dbc_path.name
        except Exception as e:
            raise Exception(f"Failed to parse DBC file {self.dbc_path}: {e}")

    def decode_message(self, msg: can.Message) -> Dict[str, Any]:
        """
        Decode a CAN message using the DBC database.

        Args:
            msg: can.Message object to decode

        Returns:
            Dictionary with:
                - 'name': Message name
                - 'signals': Dict of signal_name -> value
                - 'raw_id': CAN ID (hex)

        Raises:
            KeyError: If message ID not found in DBC
            Exception: If decoding fails
        """
        # Find message definition
        db_msg = self.dbc.get_message_by_frame_id(msg.arbitration_id)

        # Decode signals
        signals = db_msg.decode(bytes(msg.data))

        return {
            'name': db_msg.name,
            'signals': signals,
            'raw_id': f"0x{msg.arbitration_id:03X}"
        }

    def encode_message(self, message_name: str, signals: Dict[str, Any]) -> can.Message:
        """
        Encode a CAN message from signal values.

        Args:
            message_name: Name of the message (from DBC)
            signals: Dictionary of signal_name -> value

        Returns:
            can.Message object ready to send

        Raises:
            KeyError: If message or signal not found in DBC
            Exception: If encoding fails
        """
        # Find message definition
        db_msg = self.dbc.get_message_by_name(message_name)

        # Encode signals to bytes
        data = db_msg.encode(signals)

        # Create CAN message
        return can.Message(
            arbitration_id=db_msg.frame_id,
            data=data,
            is_extended_id=False
        )
    
    def get_message(self, message_name: str):
        """
        Returns the CAN message object from the DBC
        
        Args:
            message_name: Name of the message (from DBC)
            
        Returns:
            CAN message object

        Raises:
            KeyError: If message not found in DBC    
        """
        return self.dbc.get_message_by_name(message_name)

    def get_message_info(self, message_name: str) -> Dict[str, Any]:
        """
        Get information about a message from the DBC.

        Args:
            message_name: Name of the message

        Returns:
            Dictionary with message metadata:
                - 'frame_id': CAN ID
                - 'length': Data length
                - 'signals': List of signal dictionaries

        Raises:
            KeyError: If message not found
        """
        db_msg = self.dbc.get_message_by_name(message_name)

        return {
            'name': db_msg.name,
            'frame_id': f"0x{db_msg.frame_id:03X}",
            'length': db_msg.length,
            'signals': [
                {
                    'name': sig.name,
                    'unit': sig.unit or '-',
                    'min': sig.minimum,
                    'max': sig.maximum,
                    'scale': sig.scale,
                    'offset': sig.offset,
                }
                for sig in db_msg.signals
            ]
        }

    def list_messages(self) -> list:
        """
        List all messages in the DBC.

        Returns:
            List of message names
        """
        return [msg.name for msg in self.dbc.messages]

    def search_signal(self, signal_name: str) -> Optional[str]:
        """
        Find which message contains a given signal.

        Args:
            signal_name: Name of the signal to search for

        Returns:
            Message name containing the signal, or None if not found
        """
        for msg in self.dbc.messages:
            for sig in msg.signals:
                if sig.name == signal_name:
                    return msg.name
        return None
