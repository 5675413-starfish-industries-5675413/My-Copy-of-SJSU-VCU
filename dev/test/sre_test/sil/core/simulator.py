"""
SIL Simulator - Runs the compiled executable and handles JSON communication.
"""

import subprocess
import json
import threading
import queue
import time
import re
from pathlib import Path
from typing import Optional, Dict, Any, Iterator
from sre_test.sil.core.helpers.path import STRUCT_MEMBERS
from sre_test.sil.core.compiler import SILCompiler


class SILSimulator:
    """Runs the SIL executable and handles stdin/stdout JSON communication."""

    @staticmethod
    def _struct_name_to_output_key(struct_name: str) -> str:
        """Convert C struct names to JSON keys (e.g., PowerLimit -> power_limit)."""
        return re.sub(r'(?<!^)(?=[A-Z])', '_', struct_name).lower()

    def __init__(self, executable: Path):
        self.executable = executable
        self.process: Optional[subprocess.Popen] = None
        self.stdout_queue: queue.Queue = queue.Queue()
        self._reader_thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()

    @classmethod
    def create(cls, auto_compile: bool = True, verbose: bool = False):
        """
        Create and start a simulator with automatic compilation.
        
        Args:
            auto_compile: If True, compiles automatically if executable doesn't exist
            verbose: If True, shows compilation output
        
        Returns:
            SILSimulator instance (already started)
        
        Example:
            >>> sim = SILSimulator.create()
            >>> sim.send_structs("MotorController")
            >>> response = sim.receive()
            >>> sim.stop()  # Clean up when done
        """
        compiler = SILCompiler()
        
        if auto_compile or not compiler.executable.exists():
            if not compiler.compile(verbose=verbose):
                raise RuntimeError("Compilation failed")
        
        simulator = cls(compiler.executable)
        simulator.start()
        return simulator

    def start(self) -> None:
        """Start the SIL executable."""
        if not self.executable.exists():
            raise FileNotFoundError(f"Executable not found: {self.executable}")

        self.process = subprocess.Popen(
            [str(self.executable.resolve())],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=None,
            text=True,
            bufsize=1,
        )

        # Start stdout reader thread
        self._reader_thread = threading.Thread(target=self._read_stdout, daemon=True)
        self._reader_thread.start()

    def _read_stdout(self) -> None:
        """Background thread to read JSON from stdout."""
        while not self._stop_event.is_set() and self.process and self.process.poll() is None:
            try:
                line = self.process.stdout.readline()
                if not line:
                    break
                line = line.strip()
                if line:
                    try:
                        self.stdout_queue.put(json.loads(line))
                    except json.JSONDecodeError:
                        pass
            except Exception as e:
                if not self._stop_event.is_set():
                    print(f"Error reading stdout: {e}")
                break

    def send(self, data: Dict[str, Any]) -> None:
        """Send JSON data to the simulator via stdin."""
        if self.process and self.process.poll() is None:
            json_str = json.dumps(data, separators=(',', ':'))
            self.process.stdin.write(json_str + '\n')
            self.process.stdin.flush()

    def send_structs(self, *struct_names: str, row_id: int = -1, 
                     struct_members_file: Optional[Path] = None, 
                     filter_none: bool = True) -> None:
        """
        Extract and send specific structs from struct_members_output.json.
        
        Args:
            *struct_names: Full names of structs to send (e.g., "PowerLimit", "MotorController")
            row_id: Row ID to include in JSON (default: -1 for initialization)
            struct_members_file: Optional path to struct_members_output.json. 
                               If None, uses STRUCT_MEMBERS from helpers.path.
            filter_none: If True, filters out None values from parameters
        
        Example:
            >>> with SILSimulator(compiler.executable) as sim:
            >>>     sim.send_structs("PowerLimit", "MotorController")
        """
        if struct_members_file is None:
            struct_members_file = STRUCT_MEMBERS
        
        if not struct_members_file.exists():
            raise FileNotFoundError(f"Struct members file not found: {struct_members_file}")
        
        # Load struct members
        with open(struct_members_file, 'r', encoding='utf-8') as f:
            all_structs = json.load(f)
        
        # Extract requested structs
        structs_to_send = []
        for struct in all_structs:
            if isinstance(struct, dict) and struct.get('name') in struct_names:
                params = struct.get('parameters', {})
                if filter_none:
                    params = {k: v for k, v in params.items() if v is not None}
                structs_to_send.append({
                    "name": struct['name'],
                    "parameters": params
                })
        
        if not structs_to_send:
            available = [s.get('name') for s in all_structs if isinstance(s, dict) and 'name' in s]
            raise ValueError(
                f"No structs found for: {', '.join(struct_names)}. "
                f"Available structs: {', '.join(available)}"
            )
        
        # Send in format expected by C code
        json_data = {"row_id": row_id, "structs": structs_to_send}
        self.send(json_data)

    def receive(self, *struct_names: str, timeout: float = 0.25) -> Optional[Dict[str, Any]]:
        """
        Receive JSON response from simulator.
        
        Args:
            *struct_names: Optional struct names to request (e.g., "MotorController", "PowerLimit")
                          If provided, sends a request for these structs before receiving.
            timeout: Timeout in seconds for receiving response
        
        Returns:
            JSON response dictionary, or None if timeout
        
        Example:
            >>> response = sim.receive("MotorController")
            >>> response = sim.receive("MotorController", "PowerLimit")
        """
        try:
            response = self.stdout_queue.get(timeout=timeout)
            
            # If only one struct was requested, flatten the response
            # to preserve existing call-site behavior.
            if struct_names and len(struct_names) == 1 and response and isinstance(response, dict):
                key = self._struct_name_to_output_key(struct_names[0])
                if key in response:
                    return response[key]
            
            return response
        except queue.Empty:
            return None

    def stop(self) -> None:
        """Stop the simulator."""
        self._stop_event.set()
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                if self.process:
                    self.process.kill()
                    self.process.wait()

    def __enter__(self):
        """Context manager entry."""
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.stop()

